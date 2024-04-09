use super::RequestType;

use std::os::unix::io::AsRawFd; 
use std::fs::File;

use nix::ioctl_read_bad;
//use nix::sys::socket::VsockAddr;
use anyhow::{Result, anyhow};


const VSOCK_PATH: &str = "/dev/vsock";
const IOCTL_VM_SOCKETS_GET_LOCAL_CID: usize = 0x7b9;


pub struct HostBridge {
    cid: u32
}


impl HostBridge {
    pub fn new() -> Result<Self> {
        Ok(HostBridge { cid: HostBridge::get_local_cid()? })
    }

    fn get_local_cid() -> Result<u32> {
        let mut cid: u32 = 0;
        let vsock = File::open(VSOCK_PATH)?;

        ioctl_read_bad!(
            get_local_cid,
            IOCTL_VM_SOCKETS_GET_LOCAL_CID,
            u32
        );
        
        unsafe{ get_local_cid(vsock.as_raw_fd(), &mut cid) }?;
        
        Ok(cid)
    }

    pub(in super) fn do_read(&mut self) -> Result<Vec<u8>> {
        Err(anyhow!("Not implemented"))
    }

    pub(in super) fn do_write(&mut self, _request: RequestType) -> Result<()> {
        Err(anyhow!("Not implemented"))
    }

    
}