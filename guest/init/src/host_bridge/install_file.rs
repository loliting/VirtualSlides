use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
pub struct InstallFile{
    pub content: Vec<u8>,
    pub path: String,

    pub uid: u32,
    pub gid: u32,
    pub perm: u32
}