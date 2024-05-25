use std::path::Path;
use std::fs::{File, remove_file};
use std::os::unix::fs::symlink;
use std::io::{Read, Write};
use std::process::Command;
use nix::sys::reboot::{reboot as nix_reboot, RebootMode};
use nix::unistd::{sync, sethostname};
use nix::mount::{mount, MsFlags};
use anyhow::Result;

use crate::host_bridge::*;

pub fn set_hostname(hostname: String) -> Result<()> {
    sethostname(hostname.lines().next().unwrap_or("UNKNOWN"))?;
    
    Ok(())
}

pub fn set_hostname_from_file() -> Result<()> {
    let mut hostname_file = File::open("/etc/hostname")?;

    let mut hostname = String::new();
    hostname_file.read_to_string(&mut hostname)?;
    
    set_hostname(hostname)?;
    
    Ok(())
}

pub fn system_init() -> Result<()> {
    mount_sys_dirs()?;
    set_hostname_from_file()?;
    setup_symlinks()?;
    fix_term()?;

    Ok(())
}

pub fn is_first_boot() -> bool {
    return Path::new("/firstboot").exists();
}

pub fn poweroff() -> Result<()> {
    sync();
    nix_reboot(RebootMode::RB_AUTOBOOT)?;

    Ok(())
}

pub fn reboot() -> Result<()> {
    HostBridge::new()?.message_host_simple(RequestType::Reboot)?;
    poweroff()
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

pub fn first_boot_initialization() -> Result<()> {
    let mut hb = HostBridge::new()?;

    let init_scripts = match hb.message_host_simple(RequestType::GetInitScripts)?.init_scripts {
        Some(init_scripts) => init_scripts,
        None => Vec::new()
    };
    
    let install_files = match hb.message_host_simple(RequestType::GetInstallFiles)?.install_files {
        Some(install_files) => install_files,
        None => Vec::new()
    };

    let hostname = match hb.message_host_simple(RequestType::GetHostname)?.hostname {
        Some(hostname) => hostname,
        None => String::new()
    };
    
    for mut init_script in init_scripts {
        init_script.exec()?;
    }

    for mut file in install_files {
        file.install()?;
    }
    
    if !hostname.is_empty() {
        let mut hostname_file = File::options()
            .write(true)
            .create(true)
            .truncate(true)
            .open("/etc/hostname")?;
    
        hostname_file.write_all(hostname.as_bytes())?;

        set_hostname(hostname)?;
    }

    remove_file("/firstboot")?;

    Ok(())
}

fn force_symlink<S: AsRef<Path> + ?Sized>(orginal: &S, link: &S) -> Result<()> {
    if link.as_ref().exists() {
        remove_file(link)?;
    }
    symlink(orginal, link)?;
    
    Ok(())
}

pub fn setup_symlinks() -> Result<()> {
    force_symlink("/sbin/vs_init", "/sbin/reboot")?;
    force_symlink("/sbin/vs_init", "/sbin/poweroff")?;
    force_symlink("/sbin/vs_init", "/sbin/shutdown")?;
    force_symlink("/sbin/vs_init", "/sbin/vs_fixterm")?;

    Ok(())
}

pub fn fix_term() -> Result<()> {
    let mut hb = HostBridge::new()?;
    
    let (height, width): (u32, u32);
    
    width = hb.message_host_simple(RequestType::GetTermSize)?.term_width.unwrap();
    height = hb.message_host_simple(RequestType::GetTermSize)?.term_height.unwrap();
    
    /* TODO: change using stty program to appropriate ioctl() calls  */
    Command::new("stty")
        .arg("-F")
        .arg("/dev/ttyS0")
        .arg("cols")
        .arg(width.to_string()).spawn()?.wait()?;

    Command::new("stty")
        .arg("-F")
        .arg("/dev/ttyS0")
        .arg("rows")
        .arg(height.to_string()).spawn()?.wait()?;

    Ok(())
}