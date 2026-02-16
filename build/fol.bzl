"""Bazel rules for FOL-ZFC proof verification."""

FolInfo = provider(
    doc = "Information about FOL header files.",
    fields = {
        "headers": "depset of .fol.def files (transitive)",
    },
)

def _fol_library_impl(ctx):
    headers = depset(
        direct = [ctx.file.header],
        transitive = [dep[FolInfo].headers for dep in ctx.attr.deps],
    )
    return [
        FolInfo(headers = headers),
        DefaultInfo(files = depset([ctx.file.header])),
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
    },
    doc = "A FOL header library (axioms and claims, no proofs).",
)

def _fol_proof_impl(ctx):
    header_file = ctx.file.header
    proof_file = ctx.file.proof
    output = ctx.actions.declare_file(ctx.label.name + ".proven")

    # Collect all transitive headers from deps
    dep_headers = depset(transitive = [dep[FolInfo].headers for dep in ctx.attr.deps])

    # All inputs: header, proof, and transitive dep headers
    all_headers = depset(
        direct = [header_file],
        transitive = [dep_headers],
    )
    inputs = depset(
        direct = [proof_file],
        transitive = [all_headers],
    )

    # Build command: proof_checker <header> <proof> [dep_headers...]
    args = ctx.actions.args()
    args.add(header_file)
    args.add(proof_file)
    args.add_all(dep_headers)

    ctx.actions.run(
        outputs = [output],
        inputs = inputs,
        executable = ctx.executable._proof_checker,
        arguments = [args],
        env = {"FOL_OUTPUT": output.path},
        mnemonic = "FolProof",
        progress_message = "Proving %{label}",
    )

    # This target also acts as a FolInfo provider (its header is available to dependents)
    headers = depset(
        direct = [header_file],
        transitive = [dep[FolInfo].headers for dep in ctx.attr.deps],
    )

    return [
        FolInfo(headers = headers),
        DefaultInfo(files = depset([output])),
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
        "_proof_checker": attr.label(
            default = "//src/tools:proof_checker",
            executable = True,
            cfg = "exec",
        ),
    },
    doc = "Verify that all claims in a .fol.def file are proved by the .fol.proof file.",
)
