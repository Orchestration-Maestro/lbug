use super::{env, fs, Fixture};

#[cfg(unix)]
#[test]
fn hard_linked_artifact_is_refused() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    let artifact = fixture.entry().join("build/src/liblbug.a");
    fs::hard_link(&artifact, fixture.temp.path().join("outside.a")).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "hard-linked artifact was reused");
}

#[cfg(unix)]
#[test]
fn outside_response_file_edit_bypasses_cache() {
    let fixture = Fixture::new();
    let response = fixture.temp.path().join("flags.rsp");
    fs::write(&response, "-DCACHE_ANSWER=1\n").unwrap();
    env::set_var("CXXFLAGS", format!("@{}", response.display()));
    fixture.build("target-a");
    fs::write(&response, "-DCACHE_ANSWER=2\n").unwrap();
    fixture.build("target-b");
    env::remove_var("CXXFLAGS");
    assert_eq!(
        fixture.compiles(),
        2,
        "edited response file reused stale output"
    );
    assert!(!fixture.cache.exists(), "response file accessed cache");
}

#[cfg(unix)]
#[test]
fn artifact_sync_failure_leaves_nothing_published() {
    use std::{cell::Cell, io};
    let fixture = Fixture::new();
    env::set_var("OUT_DIR", fixture.temp.path().join("out"));
    let cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    let called = Cell::new(false);
    let library_size = Cell::new(0);
    let output = cache
        .get_or_build_with_sync(
            |private| {
                let built = cmake::Config::new(&fixture.root)
                    .out_dir(private)
                    .no_build_target(true)
                    .build();
                library_size.set(
                    fs::metadata(built.join("build/src/liblbug.a"))
                        .unwrap()
                        .len(),
                );
                built
            },
            |file| {
                // Fail the library data fsync, not a manifest/directory sync.
                if file.metadata()?.len() == library_size.get() {
                    called.set(true);
                    Err(io::Error::from_raw_os_error(libc::EIO))
                } else {
                    file.sync_all()
                }
            },
        )
        .unwrap();
    assert!(called.get(), "artifact data was never synchronized");
    assert!(output.join("build/src/liblbug.a").is_file());
    assert_eq!(
        fs::read_dir(&fixture.cache).unwrap().count(),
        0,
        "fsync failure left a published/private entry"
    );
}

#[cfg(unix)]
#[test]
fn link_output_survives_cache_replacement() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    env::set_var("OUT_DIR", fixture.temp.path().join("target-b"));
    let cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    let output = cache.get_or_build(|_| panic!("expected hit")).unwrap();
    let bytes = fs::read(output.join("build/src/liblbug.a")).unwrap();
    assert!(
        output.starts_with(fixture.temp.path().join("target-b")),
        "link output is not target-owned"
    );
    fs::remove_dir_all(&fixture.cache).unwrap();
    fs::create_dir(&fixture.cache).unwrap();
    assert_eq!(fs::read(output.join("build/src/liblbug.a")).unwrap(), bytes);
}

#[cfg(unix)]
#[test]
fn file_loading_flags_use_cc_tokenization() {
    let fixture = Fixture::new();
    for flag in [
        "-idirafter",
        "-iprefix",
        "-iwithprefix",
        "-iwithprefixbefore",
        "-B",
        "-L",
        "-F",
        "-iframework",
        "-fmodule-map-file=",
        "-fmodule-file=",
        "-fprebuilt-module-path=",
        "-resource-dir=",
        "-gcc-toolchain=",
        "--gcc-toolchain=",
        "-fprofile-instr-use=",
        "-fauto-profile=",
        "-fprofile-list=",
        "-Xpreprocessor",
        "-Wl,",
        "-Wa,",
        "--options-file=",
        "/FI",
        "/I",
        "/FU",
        "/AI",
        "/LIBPATH:",
    ] {
        for shell in ["0", "1"] {
            env::set_var("CC_SHELL_ESCAPED_FLAGS", shell);
            let value = if shell == "1" {
                format!("'{flag}outside path'")
            } else {
                format!("{flag}outside")
            };
            env::set_var("CXXFLAGS", value);
            let cache = crate::native_cache::NativeCache::from_env(&fixture.root).unwrap();
            assert!(
                cache.is_none(),
                "file-loading flag {flag} (shell={shell}) was cached"
            );
        }
    }
    env::set_var("CC_SHELL_ESCAPED_FLAGS", "1");
    env::set_var("CXXFLAGS", "-DCACHE_TEXT='contains -I but no input'");
    assert!(
        crate::native_cache::NativeCache::from_env(&fixture.root)
            .unwrap()
            .is_some(),
        "quoted macro text was misclassified"
    );
    env::remove_var("CC_SHELL_ESCAPED_FLAGS");
    env::remove_var("CXXFLAGS");
}

#[cfg(unix)]
#[test]
fn external_cmake_cc_roots_bypass_without_access() {
    let fixture = Fixture::new();
    for name in [
        "CMAKE_INCLUDE_PATH",
        "CMAKE_LIBRARY_PATH",
        "CMAKE_FRAMEWORK_PATH",
        "CMAKE_PROGRAM_PATH",
        "OpenSSL_ROOT",
        "WASI_SDK_PATH",
        "WASI_SYSROOT",
        "WASM_MUSL_SYSROOT",
        "PAUTHTEST_RESOURCE_DIR",
        "PAUTHTEST_SYSROOT",
    ] {
        env::set_var(name, &fixture.root);
        let result = crate::native_cache::NativeCache::from_env(&fixture.root);
        env::remove_var(name);
        assert!(result.unwrap().is_none(), "external root {name} was cached");
        assert!(
            !fixture.cache.exists(),
            "external root {name} accessed cache"
        );
    }
}

#[cfg(unix)]
#[test]
fn held_directory_permissions_are_rechecked() {
    use std::os::unix::fs::PermissionsExt;
    let fixture = Fixture::new();
    fixture.build("target-a");
    let cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    fs::set_permissions(&fixture.cache, fs::Permissions::from_mode(0o770)).unwrap();
    assert!(cache
        .get_or_build(|_| panic!("untrusted root must fail"))
        .is_err());
}

#[cfg(unix)]
#[test]
fn symlinked_artifact_is_refused() {
    use std::os::unix::fs::symlink;
    let fixture = Fixture::new();
    fixture.build("target-a");
    let artifact = fixture.entry().join("build/src/liblbug.a");
    let outside = fixture.temp.path().join("outside.a");
    fs::rename(&artifact, &outside).unwrap();
    symlink(&outside, &artifact).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "symlinked artifact was reused");
}

#[cfg(unix)]
#[test]
fn swapped_untrusted_root_is_refused() {
    use std::os::unix::fs::PermissionsExt;
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    let saved_cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    let saved_root = fixture.temp.path().join("cache-saved");
    fs::rename(&fixture.cache, &saved_root).unwrap();
    fs::create_dir(&fixture.cache).unwrap();
    fs::set_permissions(&fixture.cache, fs::Permissions::from_mode(0o770)).unwrap();
    let basename = entry.file_name().unwrap();
    fs::rename(saved_root.join(basename), fixture.cache.join(basename)).unwrap();
    let result = saved_cache.get_or_build(|_| panic!("unexpected miss"));
    let mode = fs::metadata(&fixture.cache).unwrap().permissions().mode() & 0o777;
    println!(
        "R1 observed: compile_count={}, replacement_root={}, root_mode={mode:04o}, result={result:?}",
        fixture.compiles(),
        fixture.cache.display()
    );
    assert!(
        result.is_err(),
        "untrusted replacement root was returned: {result:?}"
    );
}

#[cfg(unix)]
#[test]
fn external_idirafter_header_is_not_reused() {
    let fixture = Fixture::new();
    let headers = fixture.temp.path().join("external-headers");
    fs::create_dir(&headers).unwrap();
    fs::write(
        fixture.root.join("fixture.cpp"),
        "#include <answer.h>\nint fixture() { return CACHE_ANSWER; }\n",
    )
    .unwrap();
    let header = headers.join("answer.h");
    fs::write(&header, "#define CACHE_ANSWER 1\n").unwrap();
    let flags = format!("-idirafter {}", headers.display());
    env::set_var("CXXFLAGS", &flags);
    fixture.build("target-a");
    let first_count = fixture.compiles();
    fs::write(&header, "#define CACHE_ANSWER 2\n").unwrap();
    fixture.build("target-b");
    let count = fixture.compiles();
    let root_untouched = !fixture.cache.exists();
    println!(
        "R2 observed: CXXFLAGS={flags:?}, header={}, CACHE_ANSWER=1->2, first_compile_count={first_count}, final_compile_count={count}, cache_root_untouched={root_untouched}",
        header.display()
    );
    env::remove_var("CXXFLAGS");
    assert!(
        count == 2 || root_untouched,
        "external header change reused stale output: compile_count={count}, cache_root_untouched={root_untouched}"
    );
}

#[cfg(unix)]
#[test]
fn external_cmake_include_path_header_is_not_reused() {
    let fixture = Fixture::new();
    let headers = fixture.temp.path().join("outside-headers");
    fs::create_dir(&headers).unwrap();
    let cmake = fixture.root.join("CMakeLists.txt");
    let mut content = fs::read_to_string(&cmake).unwrap();
    content.push_str(
        "\nfind_path(ANSWER_DIR answer.h REQUIRED)\ntarget_include_directories(lbug PRIVATE \"${ANSWER_DIR}\")\n",
    );
    fs::write(cmake, content).unwrap();
    fs::write(
        fixture.root.join("fixture.cpp"),
        "#include <answer.h>\nint fixture() { return CACHE_ANSWER; }\n",
    )
    .unwrap();
    let header = headers.join("answer.h");
    fs::write(&header, "#define CACHE_ANSWER 1\n").unwrap();
    env::set_var("CMAKE_INCLUDE_PATH", &headers);
    fixture.build("target-a");
    let first_count = fixture.compiles();
    fs::write(&header, "#define CACHE_ANSWER 2\n").unwrap();
    fixture.build("target-b");
    let count = fixture.compiles();
    let bypassed = !fixture.cache.exists();
    println!(
        "R4 observed: CMAKE_INCLUDE_PATH={}, header={}, CACHE_ANSWER=1->2, first_compile_count={first_count}, final_compile_count={count}, cache_bypassed={bypassed}",
        headers.display(),
        header.display()
    );
    env::remove_var("CMAKE_INCLUDE_PATH");
    assert!(
        count == 2 || bypassed,
        "external CMake header change reused stale output: compile_count={count}, cache_bypassed={bypassed}"
    );
}
