#[path = "build_support/build_env.rs"]
mod build_env;

#[path = "build_support/native_cache.rs"]
mod native_cache;

use crate::build_env as env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn link_mode() -> &'static str {
    if env::var("LBUG_SHARED").is_ok() {
        "dylib"
    } else {
        "static"
    }
}

fn get_target() -> String {
    env::var("PROFILE").unwrap()
}

fn link_openssl() {
    for var in ["OPENSSL_DIR", "OPENSSL_ROOT_DIR"] {
        if let Ok(dir) = env::var(var) {
            let path = PathBuf::from(&dir);
            let lib_dir = path.join("lib");
            let search = if lib_dir.is_dir() { lib_dir } else { path };
            println!("cargo:rustc-link-search=native={}", search.display());
            return;
        }
    }

    match vcpkg::find_package("openssl") {
        Ok(_) => return,
        Err(e) => println!("cargo:warning=vcpkg did not find openssl: {e}"),
    }

    if let Ok(output) = Command::new("pkg-config")
        .args(["--variable=libdir", "openssl"])
        .output()
    {
        if output.status.success() {
            let lib_dir = String::from_utf8_lossy(&output.stdout).trim().to_string();
            let path = PathBuf::from(&lib_dir);
            if path.is_dir() {
                println!("cargo:rustc-link-search=native={}", path.display());
            }
        }
    }

    #[cfg(target_os = "macos")]
    {
        for prefix in [
            "/opt/homebrew/opt/openssl/lib",
            "/usr/local/opt/openssl/lib",
        ] {
            let path = PathBuf::from(prefix);
            if path.is_dir() {
                println!("cargo:rustc-link-search=native={}", path.display());
                break;
            }
        }
    }

    #[cfg(not(windows))]
    {
        for dir in ["/usr/lib", "/usr/local/lib", "/usr/lib/x86_64-linux-gnu"] {
            let path = PathBuf::from(dir);
            if path.is_dir() {
                println!("cargo:rustc-link-search=native={}", path.display());
            }
        }
    }
}

fn link_libraries(link_bundled_deps: bool) {
    // This also needs to be set by any crates using it if they want to use extensions
    if !cfg!(windows) && link_mode() == "static" {
        println!("cargo:rustc-link-arg=-rdynamic");
    }
    if cfg!(windows) && link_mode() == "dylib" {
        println!("cargo:rustc-link-lib=dylib=lbug_shared");
    } else if link_mode() == "dylib" {
        println!("cargo:rustc-link-lib={}=lbug", link_mode());
    } else if rustversion::cfg!(since(1.82)) {
        // Not bundled: a debug liblbug is gigabytes, and bundling copied it
        // into every rlib of this crate. The final link finds it by the search
        // path this script prints.
        println!("cargo:rustc-link-lib=static:+whole-archive,-bundle=lbug");
    } else {
        println!("cargo:rustc-link-lib=static=lbug");
    }
    if link_mode() == "static" {
        if cfg!(windows) {
            println!("cargo:rustc-link-lib=dylib=msvcrt");
            println!("cargo:rustc-link-lib=dylib=shell32");
            println!("cargo:rustc-link-lib=dylib=ole32");
            println!("cargo:rustc-link-lib=dylib=advapi32");
            println!("cargo:rustc-link-lib=dylib=crypt32");
            println!("cargo:rustc-link-lib=dylib=user32");
            println!("cargo:rustc-link-lib=dylib=ws2_32");
        } else if cfg!(target_os = "macos") {
            println!("cargo:rustc-link-lib=dylib=c++");
        } else {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        }

        if cfg!(feature = "extension_installer") {
            link_openssl();

            let (ssl_name, crypto_name) = if cfg!(windows) {
                ("libssl", "libcrypto")
            } else {
                ("ssl", "crypto")
            };
            println!("cargo:rustc-link-lib=dylib={ssl_name}");
            println!("cargo:rustc-link-lib=dylib={crypto_name}");
        }

        if !link_bundled_deps {
            return;
        }

        for lib in [
            "utf8proc",
            "antlr4_cypher",
            "antlr4_runtime",
            "re2",
            "fastpfor",
            "parquet",
            "thrift",
            "snappy",
            "zstd",
            "miniz",
            "mbedtls",
            "brotlidec",
            "brotlicommon",
            "lz4",
            "roaring_bitmap",
            "simsimd",
            "yyjson",
        ] {
            if rustversion::cfg!(since(1.82)) {
                println!("cargo:rustc-link-lib=static:+whole-archive={lib}");
            } else {
                println!("cargo:rustc-link-lib=static={lib}");
            }
        }
    }
}

fn manifest_dir() -> PathBuf {
    PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
}

fn emit_lbug_metadata(source: &str, lib_dir: &Path) {
    println!("cargo:rustc-env=LBUG_PRECOMPILED_SOURCE={source}");
    println!(
        "cargo:rustc-env=LBUG_PRECOMPILED_LIBRARY_DIR={}",
        lib_dir.display()
    );
}

fn get_lbug_root() -> PathBuf {
    let manifest_dir = manifest_dir();
    if let Ok(lbug_source_dir) = env::var("LBUG_SOURCE_DIR") {
        let root = PathBuf::from(lbug_source_dir);
        if root.is_symlink() || root.is_dir() {
            return root;
        }
    }

    let sibling_root = manifest_dir.join("../ladybug");
    if sibling_root.is_symlink() || sibling_root.is_dir() {
        return sibling_root;
    }

    let bundled_root = manifest_dir.join("lbug-src");
    if bundled_root.is_symlink() || bundled_root.is_dir() {
        return bundled_root;
    }
    if cfg!(windows) {
        let in_source_root = manifest_dir.join("../..");
        if in_source_root.join("CMakeLists.txt").exists() {
            return in_source_root;
        }
    }

    panic!("Bundled lbug-src is missing; provide LBUG_SOURCE_DIR for a local source checkout");
}

/// With `LBUG_REUSE_CMAKE_BUILD` set, the `CMake` build lives in one directory
/// beside the build scripts' own, named after everything that shapes it, and a
/// finished build there is reused as it is. Cargo gives this build script a new
/// `OUT_DIR` whenever the features, flags or package selection around it change
/// (coverage, a mutation run, `-p` against `--workspace`), but the C++ library
/// is the same. Only for sources that never change in place (a registry or git
/// checkout): an edited tree is not rebuilt.
fn reused_cmake_dir(lbug_root: &Path) -> Option<PathBuf> {
    env::var_os("LBUG_REUSE_CMAKE_BUILD")?;
    // OUT_DIR is <profile>/build/lbug-<hash>/out.
    let out_dir = PathBuf::from(env::var_os("OUT_DIR")?);
    let build_scripts = out_dir.parent()?.parent()?;
    let mut shape = format!(
        "{}|{}|{}|{}",
        lbug_root.display(),
        env!("CARGO_PKG_VERSION"),
        link_mode(),
        cfg!(feature = "extension_installer")
    );
    for var in env::legacy_names() {
        shape.push('|');
        shape.push_str(&env::var(var).unwrap_or_default());
    }
    // FNV-1a: a stable name, whatever the compiler that builds this script.
    let hash = shape.bytes().fold(0xcbf2_9ce4_8422_2325_u64, |hash, byte| {
        (hash ^ u64::from(byte)).wrapping_mul(0x0100_0000_01b3)
    });
    Some(build_scripts.join(format!("lbug-cmake-{hash:016x}")))
}

/// Deletes the object files under `dir`, the static archives kept.
fn remove_objects(dir: &Path) {
    let Ok(entries) = std::fs::read_dir(dir) else {
        return;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            remove_objects(&path);
        } else if path
            .extension()
            .is_some_and(|extension| extension == "o" || extension == "obj")
        {
            let _ = std::fs::remove_file(&path);
        }
    }
}

fn build_bundled_cmake() -> Vec<PathBuf> {
    env::watch();
    let lbug_root = get_lbug_root();
    let cache = native_cache::NativeCache::from_env(&lbug_root)
        .expect("Failed to prepare LBUG_NATIVE_CACHE_DIR");
    let reused_dir = if env::var_os("LBUG_NATIVE_CACHE_DIR").is_some() {
        None
    } else {
        reused_cmake_dir(&lbug_root)
    };
    let finished = reused_dir
        .as_ref()
        .map(|dir| dir.join("lbug-build-finished"));

    let mut build = cmake::Config::new(&lbug_root);
    if let Some(dir) = &reused_dir {
        build.out_dir(dir);
    }
    build
        .no_build_target(true)
        .define("BUILD_SHELL", "OFF")
        .define("BUILD_SINGLE_FILE_HEADER", "OFF")
        .define("AUTO_UPDATE_GRAMMAR", "OFF")
        .define(
            "LBUG_EXTENSION_INSTALLER",
            if cfg!(feature = "extension_installer") {
                "ON"
            } else {
                "OFF"
            },
        );
    if link_mode() == "static" {
        // The static link never uses the shared library, the largest link.
        build.define("BUILD_SHARED_LBUG", "OFF");
    }
    if cfg!(windows) {
        if Command::new("ninja")
            .arg("--version")
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status()
            .is_ok_and(|s| s.success())
        {
            build.generator("Ninja");
        }
        build.cxxflag("/EHsc");
        build.define("CMAKE_MSVC_RUNTIME_LIBRARY", "MultiThreadedDLL");
        build.define("CMAKE_POLICY_DEFAULT_CMP0091", "NEW");
    }
    if let Ok(jobs) = env::var("NUM_JOBS") {
        std::env::set_var("CMAKE_BUILD_PARALLEL_LEVEL", jobs);
    }
    let build_dir = if let Some(cache) = cache {
        cache
            .get_or_build(|private| {
                build.out_dir(private);
                build.build()
            })
            .expect("Failed to build/publish LBUG_NATIVE_CACHE_DIR")
    } else {
        match (&reused_dir, &finished) {
            (Some(dir), Some(stamp)) if stamp.exists() => dir.clone(),
            _ => build.build(),
        }
    };
    if let (Some(dir), Some(stamp)) = (&reused_dir, &finished) {
        if !stamp.exists() {
            // The archives hold every object, and a finished build never runs
            // CMake again: the objects are half the directory's size.
            remove_objects(dir);
            std::fs::write(stamp, "").expect("Failed to mark the reused CMake build finished");
        }
    }

    let lbug_lib_path = build_dir.join("build").join("src");
    println!("cargo:rustc-link-search=native={}", lbug_lib_path.display());

    vec![
        lbug_root.join("src/include"),
        build_dir.join("build/src"),
        build_dir.join("build/src/include"),
        lbug_root.join("third_party/nlohmann_json"),
        lbug_root.join("third_party/fastpfor"),
        lbug_root.join("third_party/alp/include"),
    ]
}

fn build_ffi(
    bridge_file: &str,
    out_name: &str,
    source_file: &str,
    bundled: bool,
    include_paths: &Vec<PathBuf>,
) {
    let mut build = cxx_build::bridge(bridge_file);
    build.file(source_file);

    if bundled {
        build.define("LBUG_BUNDLED", None);
    }
    if get_target() == "debug" || get_target() == "relwithdebinfo" {
        build.define("ENABLE_RUNTIME_CHECKS", "1");
    }
    if link_mode() == "static" {
        build.define("LBUG_STATIC_DEFINE", None);
    }
    build.includes(include_paths);

    println!("cargo:rerun-if-changed=include/lbug_rs.h");
    println!("cargo:rerun-if-changed=src/lbug_rs.cpp");
    println!("cargo:rerun-if-changed={bridge_file}");
    println!("cargo:rerun-if-changed={source_file}");
    if cfg!(feature = "arrow") {
        println!("cargo:rerun-if-changed=include/lbug_arrow.h");
    }
    if bundled {
        // Note that this should match the lbug-src/* entries in the package.include list in Cargo.toml
        // Unfortunately they appear to need to be specified individually since the symlink is
        // considered to be changed each time.
        println!("cargo:rerun-if-changed=lbug-src/src");
        println!("cargo:rerun-if-changed=lbug-src/cmake");
        println!("cargo:rerun-if-changed=lbug-src/third_party");
        println!("cargo:rerun-if-changed=lbug-src/CMakeLists.txt");
        println!("cargo:rerun-if-changed=lbug-src/tools/CMakeLists.txt");
    }

    if cfg!(windows) {
        build.flag("/std:c++20");
        build.flag("/MD");
    } else {
        build.flag("-std=c++2a");
    }
    build.compile(out_name);
}

fn main() {
    env::watch();
    if env::var("DOCS_RS").is_ok() {
        // Do nothing; we're just building docs and don't need the C++ library
        return;
    }

    let manifest_dir = manifest_dir();
    let mut bundled = false;
    let link_bundled_deps = false;
    let mut include_paths = vec![manifest_dir.join("include")];

    if let (Ok(lbug_lib_dir), Ok(lbug_include)) =
        (env::var("LBUG_LIBRARY_DIR"), env::var("LBUG_INCLUDE_DIR"))
    {
        println!("cargo:rustc-link-search=native={lbug_lib_dir}");
        println!("cargo:rustc-link-arg=-Wl,-rpath,{lbug_lib_dir}");
        emit_lbug_metadata("external", Path::new(&lbug_lib_dir));
        include_paths.push(Path::new(&lbug_include).to_path_buf());
    } else {
        include_paths.extend(build_bundled_cmake());
        bundled = true;
        println!("cargo:rustc-env=LBUG_PRECOMPILED_SOURCE=source");
        println!("cargo:rustc-env=LBUG_PRECOMPILED_LIBRARY_DIR=");
    }
    if link_mode() == "static" {
        link_libraries(link_bundled_deps);
    }
    build_ffi(
        "src/ffi.rs",
        "lbug_rs",
        "src/lbug_rs.cpp",
        bundled,
        &include_paths,
    );

    if cfg!(feature = "arrow") {
        build_ffi(
            "src/ffi/arrow.rs",
            "lbug_arrow_rs",
            "src/lbug_arrow.cpp",
            bundled,
            &include_paths,
        );
    }
    if link_mode() == "dylib" {
        link_libraries(link_bundled_deps);
    }
}
