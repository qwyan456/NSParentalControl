# How to use MTP

## Mac OS

```
$ brew install libmtp
$ mtp-detect
$ mtp-connect

```

## Workaround for ptpcamerad conflict

`while ; do; pgrep -lf "[p]tpcamera" && pkill -9 -f "[p]tpcamera" ; done`