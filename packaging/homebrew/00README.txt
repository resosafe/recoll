Package building file for Mac homebrew: http://brew.sh/

Install homebrew following the instructions on the home page, then copy
recoll.rb to /usr/local/Library/Formula/, then "brew install recoll" should
just work.

- You need to install python-libxml2 and python-libxslt separately
  (e.g. using pip). These are used to index libreoffice files and other
  xml-based formats (openxml, etc.)
  
