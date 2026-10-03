use super::key;
use std::{
    env,
    ffi::{OsStr, OsString},
    fs,
    process::Command,
};

struct Inputs {
    saved: Vec<(String, Option<OsString>)>,
    source: tempfile::TempDir,
    _output: tempfile::TempDir,
}

impl Inputs {
    fn new() -> Self {
        let source = tempfile::tempdir().unwrap();
        fs::write(source.path().join("fixture.cpp"), "int fixture;\n").unwrap();
        let version = Command::new("rustc").arg("-vV").output().unwrap();
        let version = String::from_utf8(version.stdout).unwrap();
        let host = version
            .lines()
            .find_map(|line| line.strip_prefix("host: "))
            .unwrap();
        let output = tempfile::tempdir().unwrap();
        let out = output.path().to_str().unwrap().to_owned();
        let mut inputs = Self {
            saved: Vec::new(),
            source,
            _output: output,
        };
        for (name, value) in [
            ("TARGET", Some(host)),
            ("HOST", Some(host)),
            ("RUSTFLAGS", None),
            ("CARGO_ENCODED_RUSTFLAGS", None),
            ("CARGO_CFG_COVERAGE", None),
            ("CARGO_CFG_COVERAGE_NIGHTLY", None),
            ("CARGO_CFG_TRYBUILD_NO_TARGET", None),
            ("CARGO_CFG_FOO", None),
            ("CARGO_CFG_COVERAGE_EXTRA", None),
        ] {
            inputs.set(name, value);
        }
        inputs.set("OUT_DIR", Some(&out));
        inputs
    }

    fn set(&mut self, name: &str, value: Option<&str>) {
        if !self.saved.iter().any(|(saved, _)| saved == name) {
            self.saved.push((name.to_owned(), env::var_os(name)));
        }
        if let Some(value) = value {
            env::set_var(name, value);
        } else {
            env::remove_var(name);
        }
    }

    fn key(&self) -> String {
        key(self.source.path()).unwrap()
    }
}

impl Drop for Inputs {
    fn drop(&mut self) {
        for (name, value) in &self.saved {
            if let Some(value) = value {
                env::set_var(name, value);
            } else {
                env::remove_var(name);
            }
        }
    }
}

#[test]
fn coverage_semver_and_plain_have_equal_native_keys() {
    let mut inputs = Inputs::new();
    let plain = inputs.key();
    for flags in [
        "",
        "-C instrument-coverage --cfg=coverage --cfg=trybuild_no_target",
        "-C instrument-coverage --cfg=coverage --cfg=coverage_nightly --cfg=trybuild_no_target",
        "-Cinstrument-coverage --cfg coverage --cfg coverage_nightly --cfg trybuild_no_target",
        "--cap-lints=allow",
        "--cap-lints allow",
    ] {
        for name in ["RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS"] {
            let value = if name == "RUSTFLAGS" {
                flags.to_owned()
            } else {
                flags.replace(' ', "\u{1f}")
            };
            inputs.set(name, Some(&value));
            for cfg in [
                "CARGO_CFG_COVERAGE",
                "CARGO_CFG_COVERAGE_NIGHTLY",
                "CARGO_CFG_TRYBUILD_NO_TARGET",
            ] {
                inputs.set(cfg, Some(""));
            }
            assert_eq!(inputs.key(), plain, "{name}: {flags}");
            inputs.set(name, None);
        }
    }
}

#[test]
fn native_unknown_and_neighbour_flags_stay_keyed() {
    let mut inputs = Inputs::new();
    let plain = inputs.key();
    for flags in [
        "-C target-feature=+avx2",
        "-C relocation-model=pic",
        "-Z future-native-option",
        "-C future-native-option",
        "--cfg foo",
        "--cfg=foo",
        "--cfg=coverage_extra",
        "--cfg=coverage_nightly_extra",
        "--cfg=trybuild_no_target_extra",
        "--cfg coverage=\"x\"",
        "--cfg=coverage_nightly=\"x\"",
        "--cfg=trybuild_no_target=\"x\"",
        "-C instrument-coverage-extra",
        "-Cinstrument-coverage=yes",
        "--cap-lints-extra=allow",
        "--codegen=instrument-coverage",
        "-Z instrument-coverage",
        "--cfg",
        "-C",
        "--cap-lints",
        "--cap-lints=",
        "--cap-lints=warn",
        "--cap-lints=linker-plugin-lto",
        "--cap-lints -x",
        "--cap-lints=forbid",
        "--cap-lints warn",
        "-C linker-plugin-lto",
        "--cfg=coverage=\"linker-plugin-lto\"",
        "--cfg=coverageSuffix",
        "--cfg=not_coverage",
        "-C --cfg=coverage",
        "--extern --cfg=coverage",
        "--unknown --cfg=coverage",
        "-Z --cfg=coverage",
    ] {
        for name in ["RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS"] {
            let value = if name == "RUSTFLAGS" {
                flags.to_owned()
            } else {
                flags.replace(' ', "\u{1f}")
            };
            inputs.set(name, Some(&value));
            assert_ne!(inputs.key(), plain, "{name}: {flags}");
            inputs.set(name, None);
        }
    }
    for (name, value) in [
        ("CARGO_CFG_FOO", ""),
        ("CARGO_CFG_COVERAGE_EXTRA", ""),
        ("CARGO_CFG_COVERAGE", "x"),
        ("CARGO_CFG_COVERAGE_NIGHTLY", "x"),
        ("CARGO_CFG_TRYBUILD_NO_TARGET", "x"),
    ] {
        inputs.set(name, Some(value));
        assert_ne!(inputs.key(), plain, "{name}={value}");
        inputs.set(name, None);
    }
}

#[test]
fn opaque_arguments_are_not_removed_beside_allowlisted_flags() {
    let mut inputs = Inputs::new();
    for opaque in [
        "-C --cfg=coverage",
        "--extern --cfg=coverage",
        "--unknown --cfg=coverage",
    ] {
        inputs.set("RUSTFLAGS", Some(opaque));
        let expected = inputs.key();
        inputs.set(
            "RUSTFLAGS",
            Some(&format!(
                "{opaque} -C instrument-coverage --cap-lints=allow"
            )),
        );
        assert_eq!(inputs.key(), expected, "{opaque}");
    }
}

#[test]
fn opaque_option_value_pairs_are_preserved_byte_for_byte() {
    for (plain, encoded) in [
        ("--unknown --cfg=coverage", "--unknown\u{1f}--cfg=coverage"),
        ("-Z --cfg=coverage", "-Z\u{1f}--cfg=coverage"),
        ("-C --cfg=coverage", "-C\u{1f}--cfg=coverage"),
    ] {
        for (name, flags) in [("RUSTFLAGS", plain), ("CARGO_ENCODED_RUSTFLAGS", encoded)] {
            assert_eq!(
                super::native_rustflags(name, OsStr::new(flags)).as_encoded_bytes(),
                flags.as_bytes(),
                "{name}: {flags:?}"
            );
        }
    }
}

#[test]
fn non_unicode_flags_stay_keyed() {
    use std::os::unix::ffi::OsStringExt;
    let mut inputs = Inputs::new();
    let plain = inputs.key();
    for name in ["RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS"] {
        env::set_var(name, OsString::from_vec(b"--cfg=coverage\xff".to_vec()));
        assert_ne!(inputs.key(), plain, "{name}");
        inputs.set(name, None);
    }
}

#[test]
fn allowlist_cannot_change_cc_prefer_clang() {
    // cc 1.4.2 lib.rs:3165–3169 / 1.5.1 lib.rs:3356–3360 search
    // the entire encoded flags for this substring, not just codegen options.
    for spelling in super::RUST_ONLY_FLAGS {
        for token in *spelling {
            assert!(!token.contains("linker-plugin-lto"));
        }
    }
    for (bare, variable) in super::RUST_ONLY_CFGS {
        assert!(!bare.contains("linker-plugin-lto"));
        assert!(!variable.contains("linker-plugin-lto"));
    }
}

#[cfg(all(target_os = "linux", target_arch = "x86_64"))]
#[test]
fn real_bootstrap_snapshots_have_equal_native_keys_and_compiler_arguments() {
    use crate::cfg_snapshots::{self, BOOTSTRAP, COVERAGE, NORMAL, SEMVER};
    let _cfgs = cfg_snapshots::Inputs::new();
    let inputs = Inputs::new();
    cfg_snapshots::apply(NORMAL, "");
    let plain = inputs.key();
    let arguments = cfg_snapshots::compiler_args();
    assert_eq!(
        env::var("CARGO_CFG_TARGET_FEATURE").unwrap(),
        "fxsr,sse,sse2"
    );
    for (snapshot, flags) in [(NORMAL, COVERAGE), (BOOTSTRAP, SEMVER)] {
        cfg_snapshots::apply(snapshot, flags);
        assert_eq!(cfg_snapshots::compiler_args(), arguments, "{flags}");
        if flags == SEMVER {
            assert_eq!(
                env::var("CARGO_CFG_TARGET_FEATURE").unwrap(),
                "fxsr,sse,sse2,x87"
            );
            assert_eq!(
                env::var("CARGO_CFG_TARGET_HAS_ATOMIC_LOAD_STORE").unwrap(),
                "16,32,64,8,ptr"
            );
        }
        assert_eq!(inputs.key(), plain, "{flags}");
    }
}

#[cfg(all(target_os = "linux", target_arch = "x86_64"))]
#[test]
fn real_bootstrap_snapshot_keeps_native_key_boundaries() {
    use crate::cfg_snapshots::{self, BOOTSTRAP, SEMVER};
    use std::os::unix::ffi::OsStringExt;
    let _cfgs = cfg_snapshots::Inputs::new();
    let mut inputs = Inputs::new();
    cfg_snapshots::apply(BOOTSTRAP, SEMVER);
    let plain = inputs.key();
    for (name, value) in [
        ("CXXFLAGS", "-DNATIVE_FLAG=1"),
        ("PROFILE", "changed-profile"),
        ("CARGO_CFG_TARGET_FEATURE", "fxsr,sse,sse2,x87,crt-static"),
        ("CARGO_CFG_TARGET_FEATURE", "fxsr,sse,sse2,x87,avx2"),
        ("CARGO_CFG_TARGET_FEATURE", "fxsr,sse,sse2,x87_extra"),
        ("CARGO_CFG_TARGET_OS", "future-os"),
        ("CARGO_CFG_TARGET_ARCH", "future-arch"),
        ("CARGO_CFG_TARGET_ABI", "future-abi"),
        ("CARGO_CFG_FUTURE_BOOTSTRAP", ""),
        ("CARGO_CFG_FMT_DEBUG_EXTRA", "full"),
        ("CARGO_CFG_TARGET_HAS_RELIABLE_F16_EXTRA", ""),
        ("CARGO_CFG_FMT_DEBUG", "future"),
        ("CARGO_CFG_OVERFLOW_CHECKS", "future"),
        ("CARGO_CFG_RELOCATION_MODEL", "future"),
        ("CARGO_CFG_UB_CHECKS", "future"),
        ("CARGO_CFG_TARGET_HAS_ATOMIC_LOAD_STORE", "future"),
        ("CARGO_CFG_TARGET_HAS_RELIABLE_F128", "future"),
        ("CARGO_CFG_TARGET_HAS_RELIABLE_F16", "future"),
        ("CARGO_CFG_TARGET_HAS_RELIABLE_F16_MATH", "future"),
        ("CARGO_CFG_TARGET_OBJECT_FORMAT", "future"),
        ("CARGO_CFG_TARGET_THREAD_LOCAL", "future"),
    ] {
        let saved = env::var(name).ok();
        inputs.set(name, Some(value));
        assert_ne!(inputs.key(), plain, "{name}={value}");
        inputs.set(name, saved.as_deref());
    }
    for features in ["x87,fxsr,sse,sse2", "fxsr,x87,sse,sse2"] {
        inputs.set("CARGO_CFG_TARGET_FEATURE", Some(features));
        assert_eq!(inputs.key(), plain, "only x87 should be removed");
    }
    for name in ["CARGO_CFG_FMT_DEBUG", "CARGO_CFG_TARGET_FEATURE"] {
        cfg_snapshots::apply(BOOTSTRAP, SEMVER);
        env::set_var(name, OsString::from_vec(b"x87\xff".to_vec()));
        assert_ne!(inputs.key(), plain, "non-Unicode {name} was ignored");
    }
    cfg_snapshots::apply(BOOTSTRAP, SEMVER);
    fs::write(inputs.source.path().join("fixture.cpp"), "int changed;\n").unwrap();
    assert_ne!(inputs.key(), plain, "source bytes were ignored");
}
