//! Cache I/O stays relative to checked, held directory descriptors.
use std::{
    ffi::{CStr, CString, OsStr, OsString},
    fs::{File, Metadata},
    io,
    os::{
        fd::{AsRawFd, FromRawFd, IntoRawFd},
        unix::{
            ffi::{OsStrExt, OsStringExt},
            fs::MetadataExt,
        },
    },
    path::Path,
};

pub(super) struct Directory(pub(super) File);

fn name(value: &OsStr) -> io::Result<CString> {
    Ok(CString::new(value.as_bytes())?)
}

pub(super) fn trusted(file: &File, directory: bool) -> io::Result<Metadata> {
    let metadata = file.metadata()?;
    // SAFETY: geteuid has no arguments or memory preconditions.
    let uid = unsafe { libc::geteuid() };
    if metadata.uid() != uid
        || metadata.mode() & 0o022 != 0
        || (directory && !metadata.is_dir())
        || (!directory && (!metadata.is_file() || metadata.nlink() != 1))
    {
        return Err(io::Error::other(
            "expected current-user-owned, non-group/other-writable regular cache entry (no links)",
        ));
    }
    Ok(metadata)
}

impl Directory {
    pub(super) fn open(path: &Path) -> io::Result<Self> {
        let name = name(path.as_os_str())?;
        // SAFETY: name is NUL terminated; a successful fd is uniquely owned.
        let fd = unsafe {
            libc::open(
                name.as_ptr(),
                libc::O_RDONLY | libc::O_DIRECTORY | libc::O_NOFOLLOW | libc::O_CLOEXEC,
            )
        };
        let file = file(fd)?;
        trusted(&file, true)?;
        Ok(Self(file))
    }

    pub(super) fn open_file(&self, value: &OsStr) -> io::Result<File> {
        let name = name(value)?;
        // SAFETY: held directory and C string remain valid throughout openat.
        let fd = unsafe {
            libc::openat(
                self.0.as_raw_fd(),
                name.as_ptr(),
                libc::O_RDONLY | libc::O_NOFOLLOW | libc::O_CLOEXEC | libc::O_NONBLOCK,
            )
        };
        file(fd)
    }

    pub(super) fn directory(&self, value: &OsStr) -> io::Result<Self> {
        let file = self.open_file(value)?;
        trusted(&file, true)?;
        Ok(Self(file))
    }

    pub(super) fn create_directory(&self, value: &OsStr) -> io::Result<Self> {
        let name = name(value)?;
        // SAFETY: held directory and NUL terminated name are valid.
        if unsafe { libc::mkdirat(self.0.as_raw_fd(), name.as_ptr(), 0o700) } != 0 {
            return Err(io::Error::last_os_error());
        }
        self.directory(value)
    }

    pub(super) fn create_file(&self, value: &OsStr) -> io::Result<File> {
        let name = name(value)?;
        // SAFETY: held directory and NUL terminated name are valid.
        let fd = unsafe {
            libc::openat(
                self.0.as_raw_fd(),
                name.as_ptr(),
                libc::O_WRONLY | libc::O_CREAT | libc::O_EXCL | libc::O_NOFOLLOW | libc::O_CLOEXEC,
                0o600,
            )
        };
        let file = file(fd)?;
        trusted(&file, false)?;
        Ok(file)
    }

    pub(super) fn names(&self) -> io::Result<Vec<OsString>> {
        // Open a fresh description: dup would share the directory offset.
        let file = self.open_file(OsStr::new("."))?;
        let fd = file.into_raw_fd();
        // SAFETY: fd is an owned directory descriptor transferred to fdopendir.
        let stream = unsafe { libc::fdopendir(fd) };
        if stream.is_null() {
            let error = io::Error::last_os_error();
            // SAFETY: fdopendir failed, so ownership remains here.
            unsafe { libc::close(fd) };
            return Err(error);
        }
        let mut names = Vec::new();
        loop {
            // SAFETY: stream is live; returned entry remains valid until next call.
            let entry = unsafe { libc::readdir(stream) };
            if entry.is_null() {
                break;
            }
            // SAFETY: readdir supplies a NUL terminated d_name.
            let bytes = unsafe { CStr::from_ptr((*entry).d_name.as_ptr()) }.to_bytes();
            if bytes != b"." && bytes != b".." {
                names.push(OsString::from_vec(bytes.to_vec()));
            }
        }
        // SAFETY: close the stream exactly once, including its descriptor.
        let result = unsafe { libc::closedir(stream) };
        if result != 0 {
            return Err(io::Error::last_os_error());
        }
        names.sort();
        Ok(names)
    }

    pub(super) fn remove_tree(&self, value: &OsStr) -> io::Result<()> {
        let dir = self.directory(value)?;
        for child in dir.names()? {
            let file = dir.open_file(&child)?;
            if file.metadata()?.is_dir() {
                dir.remove_tree(&child)?;
            } else {
                dir.unlink(&child, false)?;
            }
        }
        self.unlink(value, true)
    }

    fn unlink(&self, value: &OsStr, directory: bool) -> io::Result<()> {
        let name = name(value)?;
        // SAFETY: held directory and C string are valid.
        let result = unsafe {
            libc::unlinkat(
                self.0.as_raw_fd(),
                name.as_ptr(),
                if directory { libc::AT_REMOVEDIR } else { 0 },
            )
        };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    pub(super) fn publish(&self, source: &OsStr, destination: &OsStr) -> io::Result<()> {
        let source = name(source)?;
        let destination = name(destination)?;
        #[cfg(target_os = "linux")]
        // SAFETY: both names and the held root descriptor are valid.
        let result = unsafe {
            libc::renameat2(
                self.0.as_raw_fd(),
                source.as_ptr(),
                self.0.as_raw_fd(),
                destination.as_ptr(),
                libc::RENAME_NOREPLACE,
            )
        };
        #[cfg(target_os = "macos")]
        // SAFETY: both names and the held root descriptor are valid.
        let result = unsafe {
            libc::renameatx_np(
                self.0.as_raw_fd(),
                source.as_ptr(),
                self.0.as_raw_fd(),
                destination.as_ptr(),
                libc::RENAME_EXCL,
            )
        };
        #[cfg(not(any(target_os = "linux", target_os = "macos")))]
        return Err(io::Error::new(
            io::ErrorKind::Unsupported,
            "atomic no-replace rename unsupported",
        ));
        #[cfg(any(target_os = "linux", target_os = "macos"))]
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }
}

fn file(fd: libc::c_int) -> io::Result<File> {
    if fd < 0 {
        return Err(io::Error::last_os_error());
    }
    // SAFETY: the successful open returned a uniquely owned descriptor.
    Ok(unsafe { File::from_raw_fd(fd) })
}
