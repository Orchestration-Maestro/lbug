//! Bindings to Lbug: an in-process property graph database management system built for query speed and scalability.
//!
//! ## Example Usage
//! ```
//! use lbug::{Database, SystemConfig, Connection};
//! # use anyhow::Error;
//!
//! # fn main() -> Result<(), Error> {
//! # let temp_dir = tempfile::tempdir()?;
//! # let path = temp_dir.path().join("testdb");
//! let db = Database::new(path, SystemConfig::default())?;
//! let conn = Connection::new(&db)?;
//! conn.query("CREATE NODE TABLE Person(name STRING, age INT64, PRIMARY KEY(name));")?;
//! conn.query("CREATE (:Person {name: 'Alice', age: 25});")?;
//! conn.query("CREATE (:Person {name: 'Bob', age: 30});")?;
//!
//! let mut result = conn.query("MATCH (a:Person) RETURN a.name AS NAME, a.age AS AGE;")?;
//! println!("{}", result);
//! # temp_dir.close()?;
//! # Ok(())
//! # }
//! ```
//! ## Building
//!
//! By default, the bundled Lbug C++ library is compiled from source and statically linked.
//! The build never downloads a precompiled library or a missing source tree.
//!
//! If you want to instead link against a pre-built version of the library, the following environment
//! variables can be used to configure the build process:
//!
//! - `LBUG_SHARED`: If set, link dynamically instead of statically
//! - `LBUG_SOURCE_DIR`: Directory of a local Lbug source checkout. Otherwise use `../ladybug`
//!   when present, or the bundled `lbug-src`. Missing sources cause a build failure.
//! - `LBUG_INCLUDE_DIR`: Directory of Lbug's headers
//! - `LBUG_LIBRARY_DIR`: Directory containing Lbug's pre-built libraries.
//!
//! Example:
//! ```bash
//! lbug_prebuilt_dir=/tmp/lbug # pre-built Lbug from https://docs.ladybugdb.com/installation/#cc
//! lbug_prebuilt_dir=/path_to_lbug_source/build/release/src # Lbug built from source
//! export LBUG_LIBRARY_DIR="lbug_prebuilt_dir"
//! export LBUG_INCLUDE_DIR="lbug_prebuilt_dir"
//! export LBUG_SHARED=1
//! ```
//! On macOS:
//! ```bash
//! brew install lbug
//! export LBUG_LIBRARY_DIR=/opt/homebrew/lib
//! export LBUG_INCLUDE_DIR=/opt/homebrew/include
//! export LBUG_SHARED=1
//! ```
//!
//! ## Using Extensions
//! By default, binaries created using this library will not work with Lbug's
//! [extensions](https://docs.ladybugdb.com/extensions/) (except on Windows/MSVC, where the linker works differently).
//!
//! If you want to use extensions in binaries (binary crates or tests) using this
//! library, you will need to add the following (or a similar command; see
//! [build-scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html#rustc-link-arg))
//! to your build.rs (or create one) so that the binary
//! produced acts like a library that the extension can link with. Not doing this will produce
//! undefined symbol errors when the extension is loaded:
//!
//! ```ignore
//! println!("cargo:rustc-link-arg=-rdynamic");
//! ```

pub use connection::{Connection, PreparedStatement};
pub use database::{Database, SystemConfig};
pub use error::Error;
pub use logical_type::LogicalType;
#[cfg(feature = "arrow")]
pub use query_result::{ArrowIterator, CsrResult};
pub use query_result::{CSVOptions, QueryResult};
pub use value::{InternalID, NodeVal, RelVal, Value};

mod connection;
mod database;
mod error;
mod ffi;
mod logical_type;
mod query_result;
mod value;

/// The version of the Lbug crate as reported by Cargo's `CARGO_PKG_VERSION` environment variable
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
/// The source of the linked Lbug library selected by the build script.
///
/// This is `external` when `LBUG_LIBRARY_DIR`/`LBUG_INCLUDE_DIR` were supplied, `source` when the
/// C++ source was built. No precompiled archives are downloaded.
pub const LBUG_LIBRARY_SOURCE: &str = env!("LBUG_PRECOMPILED_SOURCE");
/// The directory containing the linked precompiled Lbug library, if one was used.
pub const LBUG_LIBRARY_DIR: &str = env!("LBUG_PRECOMPILED_LIBRARY_DIR");

/// Returns the storage version of the Lbug library
pub fn get_storage_version() -> u64 {
    crate::ffi::ffi::get_storage_version()
}

/// Returns the source of the linked Lbug library selected by the build script.
pub fn get_library_source() -> &'static str {
    LBUG_LIBRARY_SOURCE
}

/// Returns the directory containing the linked precompiled Lbug library.
///
/// This returns `None` when Lbug was built from source as part of this crate build.
pub fn get_library_dir() -> Option<&'static str> {
    if LBUG_LIBRARY_DIR.is_empty() {
        None
    } else {
        Some(LBUG_LIBRARY_DIR)
    }
}
