
http://guide.macports.org/#development.local-repositories

To use/test the macports Portfile port out of the official macports tree:

- Edit /opt/local/etc/macports/sources.conf, and insert a URL pointing to
  your local repository before the rsync one (replace <me> with your user
  name): 

    file:///Users/<me>/macports/
    rsync://rsync.macports.org/release/ports [default]

Inserting before ensures it's used before the macports one.

- The Portfile file should be copied under category/portname. e.g.:

    /Users/<me>/macports/textproc/recoll/Portfile

- After you create or update your Portfile, use the MacPorts portindex
  command in the local repository's directory to create or update the index
  of the ports in your local repository.

    cd /Users/<me>/macports
    portindex

Once the local port is added to the PortIndex, it becomes available for
searching or installation as with any other Portfile in the MacPorts tree

