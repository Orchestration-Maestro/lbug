use super::key;
use std::{env, ffi::OsString, fs, process::Command};

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
