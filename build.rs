use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let adapters_dir = manifest_dir.join("adapters");
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());

    println!("cargo:rerun-if-changed=adapters/qala_mcp_policy_adapter.c");
    println!("cargo:rerun-if-changed=adapters/grounding_shield.c");
    println!("cargo:rerun-if-changed=adapters/qala_adapters.h");
    println!("cargo:rerun-if-changed=adapters/grounding_shield.h");
    println!("cargo:rerun-if-changed=build.rs");

    // Compile C99 safety engine via cc
    cc::Build::new()
        .file("adapters/qala_mcp_policy_adapter.c")
        .file("adapters/grounding_shield.c")
        .include("adapters")
        .flag("-std=c99")
        .flag("-O2")
        .flag("-Wall")
        .flag("-Wextra")
        .compile("qala_adapters");

    // Maintain adapters/libqala_adapters.a & adapters/libdubai_adapters.a linkage parity
    let built_archive = out_dir.join("libqala_adapters.a");
    if built_archive.exists() {
        let target_archive = adapters_dir.join("libqala_adapters.a");
        let _ = fs::copy(&built_archive, &target_archive);
        let dubai_archive = adapters_dir.join("libdubai_adapters.a");
        let _ = fs::copy(&built_archive, &dubai_archive);
    }

    println!("cargo:rustc-link-search=native={}", adapters_dir.display());
    println!("cargo:rustc-link-lib=sqlite3");
    println!("cargo:rustc-link-lib=m");
}
