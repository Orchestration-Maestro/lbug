use std::{collections::BTreeSet, env::VarError, ffi::OsString};

#[derive(Clone, Copy, PartialEq)]
enum Kind {
    Watch,
    Native,
    Target,
    Compiler,
    Flags,
    TargetFile,
    Prefix,
}

impl Kind {
    fn target_specific(self) -> bool {
        matches!(
            self,
            Self::Target | Self::Compiler | Self::Flags | Self::TargetFile
        )
    }

    fn file_input(self) -> bool {
        matches!(self, Self::TargetFile)
    }
}

// One declaration per input. The third field preserves the original order and
// contents of the legacy target-local FNV shape; it is not the external key.
const INPUTS: &[(&str, Kind, bool)] = &[
    ("TARGET", Kind::Native, true),
    ("HOST", Kind::Native, true),
    ("PROFILE", Kind::Native, true),
    ("OPT_LEVEL", Kind::Native, true),
    ("DEBUG", Kind::Native, true),
    ("CC", Kind::Compiler, true),
    ("CXX", Kind::Compiler, true),
    ("CFLAGS", Kind::Flags, true),
    ("CXXFLAGS", Kind::Flags, true),
    ("DOCS_RS", Kind::Watch, false),
    ("CARGO_MANIFEST_DIR", Kind::Watch, false),
    ("CARGO_PKG_VERSION", Kind::Native, false),
    ("CARGO_TARGET_DIR", Kind::Watch, false),
    ("OUT_DIR", Kind::Watch, false),
    ("NUM_JOBS", Kind::Watch, false),
    ("CARGO_MAKEFLAGS", Kind::Watch, false),
    ("LBUG_SHARED", Kind::Native, false),
    ("LBUG_SOURCE_DIR", Kind::Watch, false),
    ("LBUG_LIBRARY_DIR", Kind::Watch, false),
    ("LBUG_INCLUDE_DIR", Kind::Watch, false),
    ("LBUG_REUSE_CMAKE_BUILD", Kind::Watch, false),
    ("LBUG_NATIVE_CACHE_DIR", Kind::Watch, false),
    ("RUSTC", Kind::Native, false),
    ("RUSTC_LINKER", Kind::Native, false),
    ("RUSTC_WRAPPER", Kind::Native, false),
    ("RUSTFLAGS", Kind::Native, false),
    ("CARGO_ENCODED_RUSTFLAGS", Kind::Native, false),
    ("CARGO_TRIM_PATHS_REMAP", Kind::Native, false),
    ("CARGO_TRIM_PATHS_SCOPE", Kind::Native, false),
    ("PATH", Kind::Native, false),
    ("SOURCE_DATE_EPOCH", Kind::Native, false),
    ("CRATE_CC_NO_DEFAULTS", Kind::Native, false),
    ("CC_ENABLE_DEBUG_OUTPUT", Kind::Native, false),
    ("CC_FORCE_DISABLE", Kind::Native, false),
    ("CC_KNOWN_WRAPPER_CUSTOM", Kind::Native, false),
    ("CC_SHELL_ESCAPED_FLAGS", Kind::Native, false),
    ("CROSS_COMPILE", Kind::Native, false),
    ("AR", Kind::Target, false),
    ("RANLIB", Kind::Target, false),
    ("ARFLAGS", Kind::Flags, false),
    ("RANLIBFLAGS", Kind::Flags, false),
    ("CPPFLAGS", Kind::Flags, false),
    ("LDFLAGS", Kind::Flags, false),
    ("ASMFLAGS", Kind::Flags, false),
    ("CXXSTDLIB", Kind::Target, false),
    ("NVCC", Kind::Target, false),
    ("MACOSX_DEPLOYMENT_TARGET", Kind::Native, false),
    ("IPHONEOS_DEPLOYMENT_TARGET", Kind::Native, false),
    ("TVOS_DEPLOYMENT_TARGET", Kind::Native, false),
    ("WATCHOS_DEPLOYMENT_TARGET", Kind::Native, false),
    ("XROS_DEPLOYMENT_TARGET", Kind::Native, false),
    ("DEVELOPER_DIR", Kind::Native, false),
    ("VIRTUAL_ENV", Kind::Native, false),
    ("CMAKE", Kind::Compiler, false),
    ("CMAKE_GENERATOR", Kind::Target, false),
    ("CMAKE_MAKE_PROGRAM", Kind::Target, false),
    ("CMAKE_BUILD_PARALLEL_LEVEL", Kind::Watch, false),
    ("CMAKE_TOOLCHAIN_FILE", Kind::TargetFile, false),
    ("CMAKE_PROJECT_INCLUDE", Kind::TargetFile, false),
    ("CMAKE_PROJECT_INCLUDE_BEFORE", Kind::TargetFile, false),
    ("CMAKE_PROJECT_TOP_LEVEL_INCLUDES", Kind::TargetFile, false),
    ("CMAKE_C_COMPILER_LAUNCHER", Kind::TargetFile, false),
    ("CMAKE_CXX_COMPILER_LAUNCHER", Kind::TargetFile, false),
    ("CMAKE_PREFIX_PATH", Kind::TargetFile, false),
    ("EMCMAKE", Kind::TargetFile, false),
    ("EMMAKE", Kind::TargetFile, false),
    ("CMAKE_ROOT", Kind::TargetFile, false),
    ("CONDA_PREFIX", Kind::TargetFile, false),
    ("SDKROOT", Kind::TargetFile, false),
    ("CPATH", Kind::TargetFile, false),
    ("C_INCLUDE_PATH", Kind::TargetFile, false),
    ("CPLUS_INCLUDE_PATH", Kind::TargetFile, false),
    ("LIBRARY_PATH", Kind::TargetFile, false),
    ("OPENSSL_DIR", Kind::TargetFile, false),
    ("OPENSSL_ROOT_DIR", Kind::TargetFile, false),
    ("PKG_CONFIG_PATH", Kind::TargetFile, false),
    ("PKG_CONFIG_LIBDIR", Kind::TargetFile, false),
    ("WASI_SYSROOT", Kind::Native, false),
    ("WASM_MUSL_SYSROOT", Kind::Native, false),
    ("PAUTHTEST_RESOURCE_DIR", Kind::Native, false),
    ("PAUTHTEST_SYSROOT", Kind::Native, false),
    ("CARGO_FEATURE_", Kind::Prefix, false),
    ("CARGO_CFG_", Kind::Prefix, false),
    ("CARGO_PROFILE_", Kind::Prefix, false),
    ("CMAKE_", Kind::Prefix, false),
];

fn kind(name: &str) -> Option<Kind> {
    if let Some((_, kind, _)) = INPUTS.iter().find(|(base, _, _)| *base == name) {
        return Some(*kind);
    }
    INPUTS.iter().find_map(|(base, kind, _)| {
        let matches = if *kind == Kind::Prefix {
            name.starts_with(base)
        } else {
            kind.target_specific()
                && (name.starts_with(&format!("{base}_"))
                    || name == format!("HOST_{base}")
                    || name == format!("TARGET_{base}"))
        };
        if matches {
            Some(*kind)
        } else {
            None
        }
    })
}

/// All project-owned environment reads pass here. Undeclared new inputs fail
/// immediately instead of silently escaping Cargo's freshness fingerprint.
pub(crate) fn var_os(name: impl AsRef<str>) -> Option<OsString> {
    let name = name.as_ref();
    assert!(
        kind(name).is_some(),
        "undeclared build environment input: {name}"
    );
    println!("cargo:rerun-if-env-changed={name}");
    println!("native-build env-read: {name}");
    std::env::var_os(name)
}

pub(crate) fn var(name: impl AsRef<str>) -> Result<String, VarError> {
    var_os(name)
        .ok_or(VarError::NotPresent)?
        .into_string()
        .map_err(VarError::NotUnicode)
}

fn names(select: impl Fn(Kind) -> bool) -> Vec<String> {
    let target = var("TARGET").unwrap_or_default();
    let mut names = BTreeSet::new();
    for (base, kind, _) in INPUTS {
        if *kind == Kind::Prefix || !select(*kind) {
            continue;
        }
        names.insert((*base).to_owned());
        if kind.target_specific() {
            names.extend([
                format!("HOST_{base}"),
                format!("TARGET_{base}"),
                format!("{base}_{target}"),
                format!("{base}_{}", target.replace(['-', '.'], "_")),
            ]);
        }
    }
    for (name, _) in std::env::vars_os() {
        if let Ok(name) = name.into_string() {
            if kind(&name).is_some_and(&select) {
                names.insert(name);
            }
        }
    }
    names.into_iter().collect()
}

/// Track even inputs behind unexecuted branches (`DOCS_RS`, external libraries,
/// disabled caching), so toggling any one of them reruns this build script.
pub(crate) fn watch() {
    for name in names(|_| true) {
        let _ = var_os(&name);
    }
}

pub(crate) fn native_names() -> Vec<String> {
    names(|kind| kind != Kind::Watch)
}

pub(crate) fn bypass_names() -> Vec<String> {
    names(Kind::file_input)
}

pub(crate) fn legacy_names() -> impl Iterator<Item = &'static str> {
    INPUTS
        .iter()
        .filter(|(_, _, legacy)| *legacy)
        .map(|(name, _, _)| *name)
}

pub(crate) fn is_flag(name: &str) -> bool {
    kind(name) == Some(Kind::Flags)
}

pub(crate) fn is_command(name: &str) -> bool {
    kind(name) == Some(Kind::Compiler)
}
