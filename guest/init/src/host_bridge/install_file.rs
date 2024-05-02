use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
pub struct InstallFile{
    pub content: String,
    pub path: String,

    pub uid: u32,
    pub gid: u32,
    pub perm: u32
}