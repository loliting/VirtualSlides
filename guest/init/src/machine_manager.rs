use std::error::Error;
use std::fs::File;
use std::io::ErrorKind;
use nix::sys::reboot::{reboot as nix_reboot, RebootMode};
use serde_json::Value;
use crate::host_bridge::*;

const CONFIG_PATH: &str = "/etc/vs.json";

pub fn is_machine_initializated() -> bool {
    let config = File::options()
        .write(true)
        .read(true)
        .open(CONFIG_PATH);
    
    if config.is_err(){
        let err = config.unwrap_err();
        if err.kind() != ErrorKind::NotFound {
            eprintln!("Failed to open: {}: {}", CONFIG_PATH, err.to_string());
        }
        return false;
    }

    let config = config.unwrap();
    
    let v = serde_json::from_reader(config);
    if v.is_err() {
        eprintln!("Failed to parse {}: {}", CONFIG_PATH, v.unwrap_err().to_string());
        return false;
    }
    let v: Value = v.unwrap();

    let is_initializated = &v["initializated"];
    if is_initializated.is_boolean() == false {
        eprintln!("Failed to parse {}: {}", CONFIG_PATH, "Field \"initializated\" is not a boolean.");
        return false;
    }

    is_initializated.as_bool().unwrap()
}

pub fn poweroff() -> Result<(), Box<dyn Error>> {
    nix_reboot(RebootMode::RB_AUTOBOOT)?;
    Ok(())
}

pub fn reboot() -> Result<(), Box<dyn Error>> {
    let mut hb = HostBridge::new()?;
    hb.message_host(Message::new_reboot()?)?;
    nix_reboot(RebootMode::RB_AUTOBOOT)?;
    Ok(())
}