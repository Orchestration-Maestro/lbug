include!("../../build.rs");
#[cfg(test)]
#[path = "cache_tests.rs"]
mod cache_tests;
#[cfg(all(test, target_os = "linux", target_arch = "x86_64"))]
mod cfg_snapshots;
