use std::fs::File;
use std::io::ErrorKind;
use nix::sys::reboot::{reboot as nix_reboot, RebootMode};
use nix::unistd::sync;
use serde_json::Value;
use anyhow::Result;

use crate::host_bridge::*;

const CONFIG_PATH: &str = "/etc/vs.json";

pub fn is_machine_initializated() -> bool {
    let config = match File::options()
        .write(false)
        .append(false)
        .read(true)
        .open(CONFIG_PATH) {
            Ok(f) => f,
            Err(err) => {
                if err.kind() != ErrorKind::NotFound {
                    eprintln!("Failed to open: {}: {:?}", CONFIG_PATH, err);
                }
                return false;
            }
    };

    let config_json: Value = match serde_json::from_reader(config) {
        Ok(v) => v,
        Err(err) => {
            eprintln!("Failed to parse {CONFIG_PATH}: {}", err.to_string());
            return false;
        }
    };

    match config_json["initializated"].as_bool() {
        Some(b) => b,
        None => {
            eprintln!("Failed to parse {CONFIG_PATH}: {}",
                "Field \"initializated\" eiher isn't a boolean or it doesn't exist.");
            false
        }
    }
}

pub fn poweroff() -> Result<()> {
    sync();
    nix_reboot(RebootMode::RB_AUTOBOOT)?;
    Ok(())
}

pub fn reboot() -> Result<()> {
    let mut hb = HostBridge::new()?;
    hb.message_host(Message::from_message_type(MessageType::Reboot))?;
    sync();
    nix_reboot(RebootMode::RB_AUTOBOOT)?;
    Ok(())
}