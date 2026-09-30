use crate::{error::Error, ffi::ffi};
use std::{fmt, path::Path};

/// A held existing root for restricted projection operations.
///
/// Opening canonicalizes the caller's absolute root once. Links in the root's own path
/// are resolved then; that is the caller's trust decision. The canonical ancestors are
/// held no-follow and their identities are revalidated before each native operation.
/// Below this root, only plain child names and single-link regular files are accepted.
/// Replaced ancestors, links below the root and invalid names fail closed.
///
/// Windows currently refuses this capability. This partial mode must not be adopted
/// until all native safety slices and both platforms are qualified.
pub struct RootDirectory {
    pub(crate) root: cxx::SharedPtr<ffi::RootDirectory>,
}

// SAFETY: native ancestors are immutable held descriptors; file identity observations
// and operations are serialized by the native capability's mutex.
unsafe impl Send for RootDirectory {}
unsafe impl Sync for RootDirectory {}

impl RootDirectory {
    /// Holds an existing absolute directory, without creating or recursively adopting it.
    /// Invalid UTF-8 paths are rejected rather than converted lossily.
    pub fn open<P: AsRef<Path>>(absolute_root: P) -> Result<Self, Error> {
        let path = absolute_root
            .as_ref()
            .to_str()
            .ok_or(Error::InvalidRootPath)?;
        Ok(Self {
            root: ffi::open_root_directory(ffi::StringView::new(path))?,
        })
    }
}

impl fmt::Debug for RootDirectory {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RootDirectory").finish_non_exhaustive()
    }
}
