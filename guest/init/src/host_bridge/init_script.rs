use serde::{Deserialize, Serialize};
use anyhow::{Ok, Result};
use std::os::unix::fs::PermissionsExt;
use std::fs::{remove_file, File, Permissions};
use std::io::Write;
use std::process::Command;

use uuid::Uuid;

#[derive(Serialize, Deserialize, Debug)]
pub struct InitScript{
    pub content: Vec<u8>,
}

impl InitScript {
    pub fn exec(&mut self) -> Result<()> {
        let path = "/tmp/".to_owned() + &Uuid::new_v4().to_string();
        {
            let mut file = File::options()
                .write(true)
                .create(true)
                .truncate(true)
                .open(&path)?;
            file.write_all(&self.content)?;
    
            file.set_permissions(Permissions::from_mode(0111))?;
        }

        Command::new(&path).env("VS_RUN", "1").spawn()?.wait()?;

        remove_file(path)?;

        Ok(())
    }
}