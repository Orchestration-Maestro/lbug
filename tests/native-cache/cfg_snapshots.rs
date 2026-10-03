use std::{
    collections::{BTreeMap, BTreeSet},
    env,
    ffi::OsString,
};

// rustc 1.98.1 (48a229cea 2026-09-01), --print cfg for
// x86_64-unknown-linux-gnu, without and with RUSTC_BOOTSTRAP=1.
pub const NORMAL: &str = include_str!("cfg-snapshots/normal.txt");
pub const BOOTSTRAP: &str = include_str!("cfg-snapshots/bootstrap.txt");
pub const COVERAGE: &str = "-C instrument-coverage --cfg=coverage --cfg=trybuild_no_target";
pub const SEMVER: &str = "--cap-lints=allow";

pub struct Inputs(
    Vec<(OsString, OsString)>,
    Option<OsString>,
    Option<OsString>,
);

impl Inputs {
    pub fn new() -> Self {
        Self(
            env::vars_os()
                .filter(|(name, _)| name.to_string_lossy().starts_with("CARGO_CFG_"))
                .collect(),
            env::var_os("RUSTFLAGS"),
            env::var_os("CARGO_ENCODED_RUSTFLAGS"),
        )
    }
}

impl Drop for Inputs {
    fn drop(&mut self) {
        for (name, _) in env::vars_os() {
            if name.to_string_lossy().starts_with("CARGO_CFG_") {
                env::remove_var(name);
            }
        }
        for (name, value) in &self.0 {
            env::set_var(name, value);
        }
        for (name, value) in [("RUSTFLAGS", &self.1), ("CARGO_ENCODED_RUSTFLAGS", &self.2)] {
            if let Some(value) = value {
                env::set_var(name, value);
            } else {
                env::remove_var(name);
            }
        }
    }
}

pub fn compiler_args() -> Vec<Vec<OsString>> {
    [false, true]
        .into_iter()
        .map(|cpp| {
            let mut build = cc::Build::new();
            build
                .cpp(cpp)
                .cargo_metadata(false)
                .opt_level(0)
                .debug(false)
                .warnings(false);
            let compiler = build.try_get_compiler().unwrap();
            compiler.args().to_vec()
        })
        .collect()
}

pub fn apply(snapshot: &str, flags: &str) {
    for (name, _) in env::vars_os() {
        if name.to_string_lossy().starts_with("CARGO_CFG_") {
            env::remove_var(name);
        }
    }
    // Cargo coalesces repeated values in sorted order and ignores the bare
    // spelling when values of that same cfg are present.
    let mut cfgs: BTreeMap<String, BTreeSet<&str>> = BTreeMap::new();
    for line in snapshot.lines() {
        let (name, value) = line.split_once('=').unwrap_or((line, ""));
        let values = cfgs
            .entry(format!("CARGO_CFG_{}", name.to_uppercase()))
            .or_default();
        if !value.is_empty() {
            values.insert(value.trim_matches('"'));
        }
    }
    for (name, values) in cfgs {
        env::set_var(name, values.into_iter().collect::<Vec<_>>().join(","));
    }
    env::remove_var("RUSTFLAGS");
    env::set_var("CARGO_ENCODED_RUSTFLAGS", flags.replace(' ', "\u{1f}"));
    if flags == COVERAGE {
        for name in ["CARGO_CFG_COVERAGE", "CARGO_CFG_TRYBUILD_NO_TARGET"] {
            env::set_var(name, "");
        }
    }
}
