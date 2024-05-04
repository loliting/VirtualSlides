use serde::{Deserialize, Serialize};
use anyhow::{Ok, Result};
use std::os::unix::fs::{chown, PermissionsExt};
use std::path::Path;
use std::fs::{File, Permissions};
use std::io::Write;

#[derive(Serialize, Deserialize, Debug)]
pub struct InstallFile{
    pub content: Vec<u8>,
    pub path: String,

    pub uid: u32,
    pub gid: u32,
    pub perm: u32
}

impl InstallFile {
    pub fn install(&mut self) -> Result<()> {
        let path = Path::new(&self.path);
        std::fs::create_dir_all(path.parent().unwrap_or(Path::new("/")))?;
        let mut file = File::options()
            .write(true)
            .create(true)
            .truncate(true)
            .open(path)?;
        file.write_all(&self.content)?;

        chown(path, Some(self.uid), Some(self.gid))?;
        file.set_permissions(Permissions::from_mode(self.perm))?;
        Ok(())
    }
}