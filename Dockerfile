FROM archlinux/base

RUN pacman -Sy --noconfirm && pacman --needed --noconfirm -S base-devel ncurses
