FROM alpine

RUN apk update && apk add --no-cache build-base ncurses-dev
