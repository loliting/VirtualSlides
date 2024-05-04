use std::fs::File;
use std::io::{ErrorKind, Read, Write};
use nix::sys::reboot::{reboot as nix_reboot, RebootMode};
use nix::unistd::{sync, sethostname};
use nix::mount::{mount, MsFlags};
use serde_json::Value;
use anyhow::Result;

use crate::host_bridge::*;

const CONFIG_PATH: &str = "/etc/vs.json";

pub fn is_machine_initializated() -> bool {
    let config = match File::open(CONFIG_PATH) {
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

pub fn set_machine_initializated(value: bool) -> Result<()> {
    let mut config = File::options()
        .write(true)
        .create(true)
        .truncate(true)
        .open(CONFIG_PATH)?;
    let message = serde_json::json!({"initializated": value});
    config.write_all(serde_json::to_string_pretty(&message)?.as_bytes())?;

    Ok(())
}

pub fn poweroff() -> Result<()> {
    sync();
    nix_reboot(RebootMode::RB_AUTOBOOT)?;

    Ok(())
}

pub fn reboot() -> Result<()> {
    let mut hb = HostBridge::new()?;
    hb.message_host(RequestType::Reboot)?;
    poweroff()
}

pub fn set_hostname() -> Result<()> {
    let mut hostname_file = File::open("/etc/hostname")?;

    let mut hostname = String::new();
    hostname_file.read_to_string(&mut hostname)?;
    
    sethostname(hostname.lines().next().unwrap_or("UNKNOWN"))?;
    
    Ok(())
}

pub fn query_hostname() -> Result<()> {
    let mut hb = HostBridge::new()?;

    let hostname = match hb.message_host(RequestType::GetHostname)?.hostname {
        Some(hostname) => hostname,
        None => String::new()
    };
    
    if !hostname.is_empty() {
        let mut hostname_file = File::options()
            .write(true)
            .create(true)
            .truncate(true)
            .open("/etc/hostname")?;
    
        hostname_file.write_all(hostname.as_bytes())?;
    }
    Ok(())
}

pub fn query_motd() -> Result<()> {
    let mut hb = HostBridge::new()?;

    let motd = match hb.message_host(RequestType::GetMotd)?.motd {
        Some(motd) => motd,
        None => String::new()
    };
    
    dbg!(&motd);

    if !motd.is_empty() {
        let mut motd_file = File::options()
            .write(true)
            .create(true)
            .truncate(true)
            .open("/etc/motd")?;
    
        motd_file.write_all(motd.as_bytes())?;
        motd_file.write(b"\n")?;
    }
    Ok(())
}

pub fn mount_sys_dirs() -> Result<()> {
    std::fs::create_dir_all("/proc")?;
    mount::<str, str, str, str>(None, "/proc", Some("proc"), MsFlags::empty(), None)?;
    
    std::fs::create_dir_all("/dev/pts")?;
    mount::<str, str, str, str>(None, "/dev/pts", Some("devpts"), MsFlags::empty(), None)?;
    
    std::fs::create_dir_all("/dev/mqueue")?;
    mount::<str, str, str, str>(None, "/dev/mqueue", Some("mqueue"), MsFlags::empty(), None)?;
    
    std::fs::create_dir_all("/dev/shm")?;
    mount::<str, str, str, str>(None, "/dev/shm", Some("tmpfs"), MsFlags::empty(), None)?;
    
    std::fs::create_dir_all("/sys")?;
    mount::<str, str, str, str>(None, "/sys", Some("sysfs"), MsFlags::empty(), None)?;
    
    Ok(())
}

pub fn install_files() -> Result<()> {
    let mut hb = HostBridge::new()?;

    let files = match hb.message_host(RequestType::GetInstallFiles)?.install_files {
        Some(install_files) => install_files,
        None => {
            return Ok(())
        }
    };

    for mut file in files {
        file.install()?;
    }

    Ok(())
}
