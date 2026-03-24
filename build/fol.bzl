"""Bazel rules for FOL-ZFC proof verification."""

FolInfo = provider(
    doc = "Information about FOL header and proof files.",
    fields = {
        "headers": "depset of .fol.def files (transitive)",
        "proofs": "depset of .fol.proof files (transitive)",
        "lib": "compiled .fol.lib file (this target's output), or None",
        "libs": "depset of .fol.lib files (transitive)",
    },
)

# ==================== Original rules (backward compat) ====================

def _fol_library_impl(ctx):
    headers = depset(
        direct = [ctx.file.header],
        transitive = [dep[FolInfo].headers for dep in ctx.attr.deps],
    )
    proofs = depset(
        transitive = [dep[FolInfo].proofs for dep in ctx.attr.deps],
    )

    # Compile header to .fol.lib via fol_compiler
    lib = ctx.actions.declare_file(ctx.label.name + ".fol.lib")
    dep_libs = depset(transitive = [dep[FolInfo].libs for dep in ctx.attr.deps if dep[FolInfo].lib])

    # Also need transitive headers for include resolution during parsing
    dep_headers = depset(transitive = [dep[FolInfo].headers for dep in ctx.attr.deps])

    args = ctx.actions.args()
    args.add(lib)              # output
    args.add(ctx.file.header)  # header
    args.add_all(dep_libs)     # dep libraries

    ctx.actions.run(
        outputs = [lib],
        inputs = depset([ctx.file.header], transitive = [dep_libs, dep_headers]),
        executable = ctx.executable._fol_compiler,
        arguments = [args],
        mnemonic = "FolCompile",
        progress_message = "Compiling %{label}",
    )

    libs = depset(
        direct = [lib],
        transitive = [dep[FolInfo].libs for dep in ctx.attr.deps if dep[FolInfo].lib],
    )

    return [
        FolInfo(headers = headers, proofs = proofs, lib = lib, libs = libs),
        DefaultInfo(files = depset([lib])),
    ]

fol_library = rule(
    implementation = _fol_library_impl,
    attrs = {
        "header": attr.label(
            allow_single_file = [".fol.def"],
            mandatory = True,
            doc = "The .fol.def header file containing axioms and claims.",
        ),
        "deps": attr.label_list(
            providers = [FolInfo],
            doc = "Other fol_library or fol_proof targets this depends on.",
        ),
        "_fol_compiler": attr.label(
            default = "//src/tools:fol_compiler",
            executable = True,
            cfg = "exec",
        ),
    },
    doc = "A FOL header library (axioms and claims, no proofs).",
)

def _fol_proof_impl(ctx):
    header_file = ctx.file.header
    proof_file = ctx.file.proof

    # Compile header + proof to .fol.lib via fol_compiler
    lib = ctx.actions.declare_file(ctx.label.name + ".fol.lib")
    proven = ctx.actions.declare_file(ctx.label.name + ".proven")
    dep_libs = depset(transitive = [dep[FolInfo].libs for dep in ctx.attr.deps if dep[FolInfo].lib])

    # Also need transitive headers for include resolution during parsing
    dep_headers = depset(transitive = [dep[FolInfo].headers for dep in ctx.attr.deps])

    args = ctx.actions.args()
    args.add(lib)           # output library
    args.add(header_file)   # header
    args.add(proof_file)    # proof file
    args.add_all(dep_libs)  # dep libraries

    ctx.actions.run(
        outputs = [lib, proven],
        inputs = depset([header_file, proof_file], transitive = [dep_libs, dep_headers]),
        executable = ctx.executable._fol_compiler,
        arguments = [args],
        env = {"FOL_OUTPUT": proven.path},
        mnemonic = "FolProof",
        progress_message = "Proving %{label}",
    )

    headers = depset(
        direct = [header_file],
        transitive = [dep[FolInfo].headers for dep in ctx.attr.deps],
    )
    proofs = depset(
        direct = [proof_file],
        transitive = [dep[FolInfo].proofs for dep in ctx.attr.deps],
    )
    libs = depset(
        direct = [lib],
        transitive = [dep[FolInfo].libs for dep in ctx.attr.deps if dep[FolInfo].lib],
    )

    return [
        FolInfo(headers = headers, proofs = proofs, lib = lib, libs = libs),
        DefaultInfo(files = depset([proven])),
    ]

fol_proof = rule(
    implementation = _fol_proof_impl,
    attrs = {
        "header": attr.label(
            allow_single_file = [".fol.def"],
            mandatory = True,
            doc = "The .fol.def header file containing axioms and claims.",
        ),
        "proof": attr.label(
            allow_single_file = [".fol.proof"],
            mandatory = True,
            doc = "The .fol.proof file containing proofs for all claims in the header.",
        ),
        "deps": attr.label_list(
            providers = [FolInfo],
            doc = "Other fol_library or fol_proof targets this depends on.",
        ),
        "_fol_compiler": attr.label(
            default = "//src/tools:fol_compiler",
            executable = True,
            cfg = "exec",
        ),
    },
    doc = "Verify that all claims in a .fol.def file are proved by the .fol.proof file.",
)
