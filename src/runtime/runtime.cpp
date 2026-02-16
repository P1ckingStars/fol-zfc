#include "runtime.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace logic {

// ========== Runtime ==========

util::Result<std::vector<ParsedStatement>> Runtime::load(std::string_view input) {
    std::string error;
    auto stmts = try_parse_statements(input, ctx_, &error);
    if (!error.empty()) {
        return MAKE_ERROR << "Parse error: " << error;
    }
    return stmts;
}

util::Result<std::vector<ParsedStatement>> Runtime::load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return MAKE_ERROR << "Could not open file: " << path;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return load(buffer.str());
}

util::Result<ParseResult> Runtime::load_with_proofs(std::string_view input) {
    std::string error;
    auto result = try_parse_with_proofs(input, ctx_, &error);
    if (!error.empty()) {
        return MAKE_ERROR << "Parse error: " << error;
    }
    return result;
}

util::Result<ParseResult> Runtime::load_file_with_proofs(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return MAKE_ERROR << "Could not open file: " << path;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_with_proofs(buffer.str());
}

util::Result<ParseResult> Runtime::load_file_recursive(const std::string& path) {
    std::unordered_set<std::string> loaded;
    std::filesystem::path abs_path = std::filesystem::absolute(path);
    std::string base_dir = abs_path.parent_path().string();
    return load_file_impl(abs_path.string(), base_dir, loaded);
}

util::Result<ParseResult> Runtime::load_file_impl(
    const std::string& path,
    const std::string& base_dir,
    std::unordered_set<std::string>& loaded) {

    // Get canonical path for cycle detection
    std::filesystem::path abs_path = std::filesystem::absolute(path);
    std::string canonical = abs_path.string();

    // Check for cycle
    if (loaded.count(canonical)) {
        return MAKE_ERROR << "Include cycle detected: " << canonical;
    }
    loaded.insert(canonical);

    // Load this file
    std::ifstream file(path);
    if (!file.is_open()) {
        return MAKE_ERROR << "Could not open file: " << path;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string error;
    auto result = try_parse_with_proofs(buffer.str(), ctx_, &error);
    if (!error.empty()) {
        return MAKE_ERROR << "Parse error in " << path << ": " << error;
    }

    // Process includes
    ParseResult merged;
    for (const auto& inc : result.includes) {
        // Resolve relative path from current file's directory
        std::filesystem::path inc_path = inc.path;
        if (inc_path.is_relative()) {
            inc_path = std::filesystem::path(base_dir) / inc_path;
        }
        std::string inc_dir = inc_path.parent_path().string();

        // Recursively load include
        auto inc_result = load_file_impl(inc_path.string(), inc_dir, loaded);
        if (!inc_result.ok()) {
            return inc_result;
        }

        // Merge include results (includes come first)
        for (auto& stmt : inc_result.value().statements) {
            merged.statements.push_back(std::move(stmt));
        }
        for (auto& proof : inc_result.value().proofs) {
            merged.proofs.push_back(std::move(proof));
        }
    }

    // Add this file's statements and proofs
    for (auto& stmt : result.statements) {
        merged.statements.push_back(std::move(stmt));
    }
    for (auto& proof : result.proofs) {
        merged.proofs.push_back(std::move(proof));
    }

    return merged;
}

// Helper to execute a single rule
static FormulaResult execute_rule(
    ProofContext& ctx,
    const ParsedProofStep& step,
    std::unordered_map<std::string, FormulaHandle>& steps,
    std::unordered_map<std::string, Term>& fixed_vars) {

    auto get_step = [&](const std::string& name) -> FormulaResult {
        auto it = steps.find(name);
        if (it == steps.end()) {
            return MAKE_ERROR << "Unknown step: " << name;
        }
        return it->second;
    };

    auto get_term = [&](const std::string& name) -> std::optional<Term> {
        auto it = fixed_vars.find(name);
        if (it != fixed_vars.end()) {
            return it->second;
        }
        return std::nullopt;
    };

    const std::string& rule = step.rule_name;
    const auto& args = step.args;

    // Conjunction
    if (rule == "and_intro") {
        if (args.size() != 2) return MAKE_ERROR << "and_intro requires 2 arguments";
        TRY_ASSIGN(a, get_step(args[0]));
        TRY_ASSIGN(b, get_step(args[1]));
        return ctx.and_intro(a, b);
    }
    if (rule == "and_elim_l") {
        if (args.size() != 1) return MAKE_ERROR << "and_elim_l requires 1 argument";
        TRY_ASSIGN(f, get_step(args[0]));
        return ctx.and_elim_l(f);
    }
    if (rule == "and_elim_r") {
        if (args.size() != 1) return MAKE_ERROR << "and_elim_r requires 1 argument";
        TRY_ASSIGN(f, get_step(args[0]));
        return ctx.and_elim_r(f);
    }

    // Disjunction
    if (rule == "or_intro_l") {
        if (args.size() != 2) return MAKE_ERROR << "or_intro_l requires 2 arguments (step, step_for_B)";
        TRY_ASSIGN(a, get_step(args[0]));
        TRY_ASSIGN(b, get_step(args[1]));
        return ctx.or_intro_l(a, b);
    }
    if (rule == "or_intro_r") {
        if (args.size() != 2) return MAKE_ERROR << "or_intro_r requires 2 arguments (step_for_A, step)";
        TRY_ASSIGN(a, get_step(args[0]));
        TRY_ASSIGN(b, get_step(args[1]));
        return ctx.or_intro_r(a, b);
    }
    if (rule == "or_elim") {
        if (args.size() != 3) return MAKE_ERROR << "or_elim requires 3 arguments";
        TRY_ASSIGN(or_f, get_step(args[0]));
        TRY_ASSIGN(a_impl_c, get_step(args[1]));
        TRY_ASSIGN(b_impl_c, get_step(args[2]));
        return ctx.or_elim(or_f, a_impl_c, b_impl_c);
    }

    // Implication
    if (rule == "implies_intro") {
        if (args.size() != 1) return MAKE_ERROR << "implies_intro requires 1 argument";
        TRY_ASSIGN(c, get_step(args[0]));
        return ctx.implies_intro(c);
    }
    if (rule == "implies_elim") {
        if (args.size() != 2) return MAKE_ERROR << "implies_elim requires 2 arguments";
        TRY_ASSIGN(impl, get_step(args[0]));
        TRY_ASSIGN(ant, get_step(args[1]));
        return ctx.implies_elim(impl, ant);
    }

    // Negation
    if (rule == "not_intro") {
        if (args.size() != 1) return MAKE_ERROR << "not_intro requires 1 argument";
        TRY_ASSIGN(bot, get_step(args[0]));
        return ctx.not_intro(bot);
    }
    if (rule == "not_elim") {
        if (args.size() != 2) return MAKE_ERROR << "not_elim requires 2 arguments";
        TRY_ASSIGN(neg, get_step(args[0]));
        TRY_ASSIGN(f, get_step(args[1]));
        return ctx.not_elim(neg, f);
    }

    // Bottom
    if (rule == "bottom_elim") {
        if (args.size() != 2) return MAKE_ERROR << "bottom_elim requires 2 arguments";
        TRY_ASSIGN(bot, get_step(args[0]));
        TRY_ASSIGN(f, get_step(args[1]));
        return ctx.bottom_elim(bot, f);
    }

    // Biconditional
    if (rule == "iff_intro") {
        if (args.size() != 2) return MAKE_ERROR << "iff_intro requires 2 arguments";
        TRY_ASSIGN(ab, get_step(args[0]));
        TRY_ASSIGN(ba, get_step(args[1]));
        return ctx.iff_intro(ab, ba);
    }
    if (rule == "iff_elim_l") {
        if (args.size() != 2) return MAKE_ERROR << "iff_elim_l requires 2 arguments";
        TRY_ASSIGN(iff, get_step(args[0]));
        TRY_ASSIGN(a, get_step(args[1]));
        return ctx.iff_elim_l(iff, a);
    }
    if (rule == "iff_elim_r") {
        if (args.size() != 2) return MAKE_ERROR << "iff_elim_r requires 2 arguments";
        TRY_ASSIGN(iff, get_step(args[0]));
        TRY_ASSIGN(b, get_step(args[1]));
        return ctx.iff_elim_r(iff, b);
    }

    // Quantifiers
    if (rule == "forall_intro") {
        if (args.size() != 1) return MAKE_ERROR << "forall_intro requires 1 argument";
        TRY_ASSIGN(body, get_step(args[0]));
        return ctx.forall_intro(body);
    }
    if (rule == "forall_elim") {
        if (args.size() != 2) return MAKE_ERROR << "forall_elim requires 2 arguments";
        TRY_ASSIGN(f, get_step(args[0]));
        auto term = get_term(args[1]);
        if (!term.has_value()) {
            return MAKE_ERROR << "Unknown term/variable: " << args[1];
        }
        return ctx.forall_elim(f, term.value());
    }
    if (rule == "exists_intro") {
        if (args.size() == 1) {
            TRY_ASSIGN(body, get_step(args[0]));
            return ctx.exists_intro(body);
        } else if (args.size() == 2) {
            TRY_ASSIGN(body, get_step(args[0]));
            auto term = get_term(args[1]);
            if (!term.has_value()) {
                return MAKE_ERROR << "Unknown term/variable: " << args[1];
            }
            return ctx.exists_intro(body, term.value());
        } else {
            return MAKE_ERROR << "exists_intro requires 1-2 arguments";
        }
    }
    if (rule == "exists_elim") {
        if (args.size() < 1 || args.size() > 2) return MAKE_ERROR << "exists_elim requires 1-2 arguments";
        TRY_ASSIGN(f, get_step(args[0]));
        return ctx.exists_elim(f);
    }

    // Classical
    if (rule == "double_neg_elim") {
        if (args.size() != 1) return MAKE_ERROR << "double_neg_elim requires 1 argument";
        TRY_ASSIGN(dn, get_step(args[0]));
        return ctx.double_neg_elim(dn);
    }
    if (rule == "excluded_middle") {
        if (args.size() != 1) return MAKE_ERROR << "excluded_middle requires 1 argument";
        TRY_ASSIGN(f, get_step(args[0]));
        return ctx.excluded_middle(f);
    }

    // Equality substitution
    if (rule == "eq_subst") {
        if (args.size() != 2) return MAKE_ERROR << "eq_subst requires 2 arguments";
        TRY_ASSIGN(eq, get_step(args[0]));
        TRY_ASSIGN(target, get_step(args[1]));
        return ctx.eq_subst(eq, target);
    }

    return MAKE_ERROR << "Unknown rule: " << rule;
}

util::ResultStatus Runtime::execute_proof(const ParsedProof& proof) {
    auto pctx = prove(proof.claim_name);

    std::unordered_map<std::string, FormulaHandle> steps;
    std::unordered_map<std::string, Term> fixed_vars;

    for (size_t i = 0; i < proof.steps.size(); ++i) {
        const auto& step = proof.steps[i];
        std::string step_desc = "Step " + std::to_string(i + 1);
        if (!step.result_name.empty()) {
            step_desc += " (" + step.result_name + ")";
        }

        switch (step.kind) {
            case ParsedProofStep::Kind::Fix: {
                if (step.args.empty()) {
                    return MAKE_ERROR << step_desc << ": fix requires variable name";
                }
                Term v = pctx.fix_var();
                fixed_vars[step.args[0]] = v;
                break;
            }

            case ParsedProofStep::Kind::Assume: {
                if (!step.formula_ast) {
                    return MAKE_ERROR << step_desc << ": assume requires formula";
                }
                // Parse formula with fixed variables in scope
                FormulaHandle assumed = parse_formula_with_vars(
                    step.formula_ast.get(), ctx_, pctx.builder(), fixed_vars);
                FormulaHandle h = pctx.assume(assumed);
                steps[step.result_name] = h;
                break;
            }

            case ParsedProofStep::Kind::Let: {
                if (!step.formula_ast) {
                    return MAKE_ERROR << step_desc << ": let requires formula";
                }
                // Parse formula with fixed variables in scope (don't assume it)
                FormulaHandle h = parse_formula_with_vars(
                    step.formula_ast.get(), ctx_, pctx.builder(), fixed_vars);
                steps[step.result_name] = h;
                break;
            }

            case ParsedProofStep::Kind::Use: {
                auto result = pctx.use(step.rule_name);
                if (!result.ok()) {
                    return MAKE_ERROR << step_desc << ": " << result.error().to_string();
                }
                steps[step.result_name] = result.value();
                break;
            }

            case ParsedProofStep::Kind::Rule: {
                auto result = execute_rule(pctx, step, steps, fixed_vars);
                if (!result.ok()) {
                    return MAKE_ERROR << step_desc << " (" << step.rule_name << "): "
                                      << result.error().to_string();
                }
                steps[step.result_name] = result.value();
                // Store witness variable for exists_elim with named witness
                if (step.rule_name == "exists_elim" && step.args.size() == 2) {
                    auto witness = pctx.last_witness();
                    if (witness.has_value()) {
                        fixed_vars[step.args[1]] = witness.value();
                    }
                }
                break;
            }

            case ParsedProofStep::Kind::Qed: {
                if (step.args.empty()) {
                    return MAKE_ERROR << step_desc << ": qed requires step name";
                }
                auto it = steps.find(step.args[0]);
                if (it == steps.end()) {
                    return MAKE_ERROR << step_desc << ": unknown step: " << step.args[0];
                }
                auto result = pctx.qed(it->second);
                if (!result.ok()) {
                    return MAKE_ERROR << step_desc << ": " << result.error().to_string();
                }
                return util::Ok();
            }
        }
    }

    return MAKE_ERROR << "Proof missing qed";
}

util::ResultStatus Runtime::execute_all_proofs(const ParseResult& result) {
    for (const auto& proof : result.proofs) {
        auto status = execute_proof(proof);
        if (!status.ok()) {
            return MAKE_ERROR << "Failed to prove '" << proof.claim_name << "': "
                              << status.error().to_string();
        }
    }
    return util::Ok();
}

ProofContext Runtime::prove(const std::string& claim_name) {
    // First try to find as a claim, then as a known axiom/theorem
    auto claim = ctx_.find_claim(claim_name);
    if (!claim.has_value()) {
        claim = ctx_.find_known(claim_name);
    }
    SentenceHandle goal = claim.value_or(SentenceHandle{});
    return ProofContext(*this, claim_name, goal);
}

ProofContext Runtime::prove(SentenceHandle goal) {
    return ProofContext(*this, "", goal);
}

// ========== ProofContext ==========

ProofContext::ProofContext(Runtime& rt, const std::string& name, SentenceHandle goal)
    : runtime_(rt), stack_(rt.context()), goal_(goal), name_(name) {}

FormulaHandle ProofContext::parse(std::string_view input) {
    SentenceHandle s = parse_sentence(input, runtime_.context());
    return builder().add_sentence(s);
}

FormulaResult ProofContext::use(const std::string& name) {
    auto found = runtime_.context().find_known(name);
    if (!found.has_value()) {
        return MAKE_ERROR << "Unknown axiom/theorem: " << name;
    }
    SentenceHandle s = found.value();
    return stack_.use_theorem(s);
}

// Scope management
Term ProofContext::fix_var() { return stack_.fix_var(); }
FormulaHandle ProofContext::assume(FormulaHandle const& f) { return stack_.assume(f); }
void ProofContext::pop() { stack_.pop(); }

// Conjunction
FormulaResult ProofContext::and_intro(FormulaHandle const& a, FormulaHandle const& b) {
    return stack_.and_intro(a, b);
}
FormulaResult ProofContext::and_elim_l(FormulaHandle const& f) {
    return stack_.and_elim_l(f);
}
FormulaResult ProofContext::and_elim_r(FormulaHandle const& f) {
    return stack_.and_elim_r(f);
}

// Disjunction
FormulaResult ProofContext::or_intro_l(FormulaHandle const& a, FormulaHandle const& b) {
    return stack_.or_intro_l(a, b);
}
FormulaResult ProofContext::or_intro_r(FormulaHandle const& a, FormulaHandle const& b) {
    return stack_.or_intro_r(a, b);
}
FormulaResult ProofContext::or_elim(FormulaHandle const& or_f, FormulaHandle const& a_impl_c, FormulaHandle const& b_impl_c) {
    return stack_.or_elim(or_f, a_impl_c, b_impl_c);
}

// Implication
FormulaResult ProofContext::implies_intro(FormulaHandle const& c) {
    return stack_.implies_intro(c);
}
FormulaResult ProofContext::implies_elim(FormulaHandle const& impl, FormulaHandle const& ant) {
    return stack_.implies_elim(impl, ant);
}

// Negation
FormulaResult ProofContext::not_intro(FormulaHandle const& b) {
    return stack_.not_intro(b);
}
FormulaResult ProofContext::not_elim(FormulaHandle const& neg, FormulaHandle const& f) {
    return stack_.not_elim(neg, f);
}

// Bottom
FormulaResult ProofContext::bottom_elim(FormulaHandle const& b, FormulaHandle const& f) {
    return stack_.bottom_elim(b, f);
}

// Biconditional
FormulaResult ProofContext::iff_intro(FormulaHandle const& ab, FormulaHandle const& ba) {
    return stack_.iff_intro(ab, ba);
}
FormulaResult ProofContext::iff_elim_l(FormulaHandle const& iff, FormulaHandle const& a) {
    return stack_.iff_elim_l(iff, a);
}
FormulaResult ProofContext::iff_elim_r(FormulaHandle const& iff, FormulaHandle const& b) {
    return stack_.iff_elim_r(iff, b);
}

// Quantifiers
FormulaResult ProofContext::forall_intro(FormulaHandle const& body) {
    return stack_.forall_intro(body);
}
FormulaResult ProofContext::forall_elim(FormulaHandle const& f, Term const& t) {
    return stack_.forall_elim(f, t);
}
FormulaResult ProofContext::exists_intro(FormulaHandle const& body, std::optional<Term> w) {
    return stack_.exists_intro(body, w);
}
FormulaResult ProofContext::exists_elim(FormulaHandle const& f) {
    return stack_.exists_elim(f);
}

// Equality
FormulaResult ProofContext::eq_subst(FormulaHandle const& eq, FormulaHandle const& target) {
    return stack_.eq_subst(eq, target);
}

// Classical extensions
FormulaResult ProofContext::double_neg_elim(FormulaHandle const& dn) {
    return stack_.double_neg_elim(dn);
}
FormulaResult ProofContext::excluded_middle(FormulaHandle const& f) {
    return stack_.excluded_middle(f);
}
FormulaResult ProofContext::classical_absurd(FormulaHandle const& b) {
    return stack_.classical_absurd(b);
}
FormulaResult ProofContext::peirce(FormulaHandle const& a, FormulaHandle const& b) {
    return stack_.peirce(a, b);
}

util::ResultStatus ProofContext::qed(FormulaHandle const& derived) {
    if (completed_) {
        return MAKE_ERROR << "Proof already completed";
    }

    // Check that the formula is actually derived (not just created via let)
    if (!stack_.is_derived(derived)) {
        return MAKE_ERROR << "qed: formula not derived: " << derived.get().to_string();
    }

    // Check that derived matches goal (by string comparison for now)
    if (goal_.valid() && derived.get().to_string() != goal_.get().to_string()) {
        return MAKE_ERROR << "Derived formula doesn't match goal\n"
                          << "  Expected: " << goal_.get().to_string() << "\n"
                          << "  Got: " << derived.get().to_string();
    }

    // Register as theorem if named
    if (!name_.empty() && goal_.valid()) {
        runtime_.context().add_theorem(name_, goal_);
    }

    completed_ = true;
    return util::Ok();
}

}  // namespace logic
