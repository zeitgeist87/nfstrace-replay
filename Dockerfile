FROM archlinux/base

RUN pacman -Sy --noconfirm && pacman --noconfirm -S base-devel ncurses
