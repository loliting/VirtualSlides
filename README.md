# VirtualSlides

## WIP - This program is not yet suited to day-to-day usage. Any new commits may contain breaking changes.

---

## Building

### Dependencies
#### App:
 - Qt6
 - CMake
 - libzip
 - qtermwidget
 - qemu (runtime dependency)
#### Guest image (and init system)
 - bash compatible shell
 - Docker (with docker buildkit)
 - Rust compiler
 - Cross
 - Cargo
 - virt-make-fs (guestfs-tools)
#### Guest kernel
 - bash compatible shell
 - wget
 - All utilities required to build linux kernel


## Virtual Slides - Presentation viewer with a dedicated virtual environment
Virtual Slides is a presentation viewer format with it's own presentation format (.vslides)
that features possiblility to add virtual networks with QEMU microVM guests inside presentations.
It's great for learning networking & linux in dedicated environment. 
In most cases guests start in <5s and have memory footprint <100MiB which creates possiblity to experiment with linux & networking without long waiting times and good hardware resources.
Guests are build from dockerfiles, which makes it easy to create new ones.
IaC configuration of exsting guest images on their first boot.
### Features:
 - Viewing simple presentations
 - Creating seperate (isolated or not) virtual networks
 - Small resource usage
 - Tasks (custom "achievements" in a VM specifies in virt-env.jsonc file)
 - Highly configurable
 - Multiple guest types and option to easly create new ones
 - Ability to modify (add new files or override config files) an existing guest image on machine's first boot

### Task overview
Virtual Slides allows presentation maker to add to their presentation custom excercies (called taks), that require to do something in the VM.  
Tasks should have description informating the user what they should do, as well as subtasks that have to be done by the user.
Because there often is many different ways to archive something in Linux, tasks can have different paths that require only a subset of the subtasks to be done for the task to be marked as completed.

#### Subtask types:
 - File task - regex match or simple match
 - Command task - kernel-level check if a certain command is executed with specified (or not) arguments and exit code.
 - (to be implemented) Script check - script to be run on guest in a periodic interval (or when a certain event occurs). If it's exit code is 0, the task is marked as succeded.

### IaC abilities 
 - Install files on guest machines
 - Executing scripts on first boot
 - Setting hostname