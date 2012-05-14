
import sys
import subprocess
import time

from gi.repository import GLib, GObject, Gio
from gi.repository import Dee
from gi.repository import Unity

import recoll

BUS_PATH = "/org/recoll/unitylensrecoll/scope/main"

# These category ids must match the order in which we add them to the lens
CATEGORY_ALL = 0

# typing timeout: we don't want to start a search for every
# char? Unity does batch on its side, but we may want more control ?
# Or not ? I'm not sure this does any good on a moderate size index.
# Set to 0 to not use it (default). Kept around because this still might be
# useful with a very big index ?
TYPING_TIMEOUT = 0

class Scope (Unity.Scope):

    def __init__ (self):
        Unity.Scope.__init__ (self, dbus_path=BUS_PATH)
        
        # Listen for changes and requests
        self.connect ("activate-uri", self.activate_uri)
        if Unity._version == "4.0":
            #print "Setting up for Unity 4.0"
            self.connect("notify::active-search",
                     self._on_search_changed)
            self.connect("notify::active-global-search",
                     self._on_global_search_changed)
            self.connect("filters-changed",
                     self._on_search_changed);
        else:
            #print "Setting up for Unity 5.0+"
            self.connect ("search-changed", self._on_search_changed)
            self.connect ("filters-changed",
                      self._on_filters_changed)

        self.last_connect_time = 0
        self.timeout_id = None

    def _connect_db(self):
        #print "Connecting to db"
        self.db = None
        try:
            self.db = recoll.connect()
            self.db.setAbstractParams(maxchars=200, contextwords=4)
        except:
            return

    def get_search_string (self):
        search = self.props.active_search
        return search.props.search_string if search else None
    
    def get_global_search_string (self):
        search = self.props.active_global_search
        return search.props.search_string if search else None
    
    def search_finished (self):
        search = self.props.active_search
        if search:
            search.emit("finished")
    
    def global_search_finished (self):
        search = self.props.active_global_search
        if search:
            search.emit("finished")

    def reset (self):
        self._do_browse (self.props.results_model)
        self._do_browse (self.props.global_results_model)
    
    def _on_filters_changed (self, scope):
        #print "_on_filters_changed()"
        self.queue_search_changed(Unity.SearchType.DEFAULT)
    
    if Unity._version == "4.0":
        def _on_search_changed (self, scope, param_spec=None):
            search_string = self.get_search_string()
            results = scope.props.results_model
            #print "Search 4.0 changed to: '%s'" % search_string
            self._update_results_model (search_string, results)
    else:
        def _on_search_changed (self, scope, search, search_type, cancellable):
            search_string = search.props.search_string
            results = search.props.results_model
            #print "Search 5.0 changed to: '%s'" % search_string
            if search_string:
                self._update_results_model (search_string, results)
            else:
                search.props.results_model.clear()
    
    def _on_global_search_changed (self, scope, param_spec):
        search = self.get_global_search_string()
        results = scope.props.global_results_model
        
        #print "Global search changed to: '%s'" % search
        
        self._update_results_model (search, results)
        
    def _update_results_model (self, search_string, model):
        if search_string:
            self._do_search (search_string, model)
        else:
            self._do_browse (model)
    
    def _do_browse (self, model):
        if self.timeout_id is not None:
            GObject.source_remove(self.timeout_id)
        model.clear ()

        if model is self.props.results_model:
            self.search_finished()
        else:
            self.global_search_finished()

    def _on_timeout(self, search_string, model):
        if self.timeout_id is not None:
            GObject.source_remove(self.timeout_id)
        self.timeout_id = None
        self._really_do_search(search_string, model)
        if model is self.props.results_model:
            self.search_finished()
        else:
            self.global_search_finished()

    def _do_search (self, search_string, model):
        if TYPING_TIMEOUT == 0:
            self._really_do_search(search_string, model)
            return True
        
        if self.timeout_id is not None:
            GObject.source_remove(self.timeout_id)
        self.timeout_id = \
            GObject.timeout_add(TYPING_TIMEOUT, self._on_timeout, 
                    search_string, model)

    def _really_do_search(self, search_string, model):
        #print "really_do_search:", "[" + search_string + "]"

        model.clear ()
        if search_string == "":
            return True

        current_time = time.time()
        if current_time - self.last_connect_time > 10:
            self._connect_db()
            self.last_connect_time = current_time

        if not self.db:
            model.append ("",
                      "error",
                      CATEGORY_ALL,
                      "text/plain",
                  "You need to use the recoll GUI to create the index first !",
                      "",
                      "")
            return
        fcat = self.get_filter("rclcat")
        for option in fcat.options:
            if option.props.active:
                search_string += " rclcat:" + option.props.id 

        # Do the recoll thing
        try:
            query = self.db.query()
            nres = query.execute(search_string)
        except:
            return

        actual_results = 0
        while query.next >= 0 and query.next < nres: 
            try:
                doc = query.fetchone()
            except:
                break

            # Results with an ipath get a special mime type so that they
            # get opened by starting a recoll instance.
            if doc.ipath != "":
                mimetype = "application/x-recoll"
                url = doc.url + "#" + doc.ipath
            else:
                mimetype = doc.mimetype
                url = doc.url

            #print "Recoll Lens: Using MIMETYPE", mimetype, " URL", url

            titleorfilename = doc.title
            if titleorfilename == "":
                titleorfilename = doc.filename

            icon = Gio.content_type_get_icon(doc.mimetype)
            if icon:
                iconname = icon.get_names()[0]

            try:
                abstract = self.db.makeDocAbstract(doc, query).encode('utf-8')
            except:
                break

            model.append (url,
                      iconname,
                      CATEGORY_ALL,
                      mimetype,
                      titleorfilename,
                      abstract,
                      doc.url)

            actual_results += 1
            if actual_results >= 20:
                break
        

    # If we return from here, the caller gets an error:
    #  Warning: g_object_get_qdata: assertion `G_IS_OBJECT (object)' failed
    # Known bug, see:
    #   https://bugs.launchpad.net/unity/+bug/893688
    # Then, the default url activation takes place
    # which is not at all what we want.
    # First workaround:
    #    In the recoll case, we just exit, the lens will be restarted.
    #    In the regular case, we return, and activation works exactly once for
    #    2 calls on oneiric and mostly for precise...
    # New workaround, suggested somewhere on the net and kept: other
    # construction method
    def activate_uri (self, scope, uri):
        """Activation handler for uri"""
        
        #print "Activate: %s" % uri
        
        # Pass all uri without fragments to the desktop handler
        if uri.find("#") == -1:
            # Reset browsing state when an app is launched
            if Unity._version == "4.0":
                self.reset ()
            ret = Unity.ActivationResponse(handled=Unity.HandledType.NOT_HANDLED,
                                                goto_uri=uri)
            return ret
        
        # Pass all others to recoll
        proc = subprocess.Popen(["recoll", uri])
        #print "Subprocess returned, going back to unity"

        scope.props.results_model.clear();
        scope.props.global_results_model.clear();
        # Old workaround:
        #sys.exit(0)

        # New and better:
        # The goto_uri thing is a workaround suggested somewhere instead of
        # passing the string. Does fix the issue
        #return Unity.ActivationResponse.new(Unity.HandledType.HIDE_DASH,''
        ret = Unity.ActivationResponse(handled=Unity.HandledType.HIDE_DASH,
                                       goto_uri='')
        return ret
