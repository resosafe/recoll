QT       -= core gui

TARGET = libxapian
TEMPLATE = lib

DEFINES += HAVE_CONFIG_H

windows {
    XAPIANDIR = ../../../../recolldeps/msvc/xapian-core/
    DEFINES += __WIN32__
    DEFINES += UNICODE
}
mac {
    XAPIANDIR = ../../../../xapian-core-1.4.18/
    CONFIG += staticlib
    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
}

SOURCES += \
$$XAPIANDIR/api/compactor.cc \
$$XAPIANDIR/api/constinfo.cc \
$$XAPIANDIR/api/decvalwtsource.cc \
$$XAPIANDIR/api/documentvaluelist.cc \
$$XAPIANDIR/api/editdistance.cc \
$$XAPIANDIR/api/emptypostlist.cc \
$$XAPIANDIR/api/error.cc \
$$XAPIANDIR/api/errorhandler.cc \
$$XAPIANDIR/api/expanddecider.cc \
$$XAPIANDIR/api/keymaker.cc \
$$XAPIANDIR/api/leafpostlist.cc \
$$XAPIANDIR/api/matchspy.cc \
$$XAPIANDIR/api/omdatabase.cc \
$$XAPIANDIR/api/omdocument.cc \
$$XAPIANDIR/api/omenquire.cc \
$$XAPIANDIR/api/positioniterator.cc \
$$XAPIANDIR/api/postingiterator.cc \
$$XAPIANDIR/api/postingsource.cc \
$$XAPIANDIR/api/postlist.cc \
$$XAPIANDIR/api/query.cc \
$$XAPIANDIR/api/queryinternal.cc \
$$XAPIANDIR/api/registry.cc \
$$XAPIANDIR/api/smallvector.cc \
$$XAPIANDIR/api/sortable-serialise.cc \
$$XAPIANDIR/api/termiterator.cc \
$$XAPIANDIR/api/termlist.cc \
$$XAPIANDIR/api/valueiterator.cc \
$$XAPIANDIR/api/valuerangeproc.cc \
$$XAPIANDIR/api/valuesetmatchdecider.cc \
$$XAPIANDIR/api/vectortermlist.cc \
$$XAPIANDIR/api/replication.cc \
$$XAPIANDIR/backends/alltermslist.cc \
$$XAPIANDIR/backends/dbcheck.cc \
$$XAPIANDIR/backends/database.cc \
$$XAPIANDIR/backends/databasehelpers.cc \
$$XAPIANDIR/backends/databasereplicator.cc \
$$XAPIANDIR/backends/dbfactory.cc \
$$XAPIANDIR/backends/slowvaluelist.cc \
$$XAPIANDIR/backends/uuids.cc \
$$XAPIANDIR/backends/valuelist.cc \
$$XAPIANDIR/backends/dbfactory_remote.cc \
$$XAPIANDIR/backends/contiguousalldocspostlist.cc \
$$XAPIANDIR/backends/flint_lock.cc \
$$XAPIANDIR/backends/chert/chert_alldocsmodifiedpostlist.cc \
$$XAPIANDIR/backends/chert/chert_alldocspostlist.cc \
$$XAPIANDIR/backends/chert/chert_alltermslist.cc \
$$XAPIANDIR/backends/chert/chert_btreebase.cc \
$$XAPIANDIR/backends/chert/chert_check.cc \
$$XAPIANDIR/backends/chert/chert_compact.cc \
$$XAPIANDIR/backends/chert/chert_cursor.cc \
$$XAPIANDIR/backends/chert/chert_database.cc \
$$XAPIANDIR/backends/chert/chert_dbcheck.cc \
$$XAPIANDIR/backends/chert/chert_dbstats.cc \
$$XAPIANDIR/backends/chert/chert_document.cc \
$$XAPIANDIR/backends/chert/chert_metadata.cc \
$$XAPIANDIR/backends/chert/chert_modifiedpostlist.cc \
$$XAPIANDIR/backends/chert/chert_positionlist.cc \
$$XAPIANDIR/backends/chert/chert_postlist.cc \
$$XAPIANDIR/backends/chert/chert_record.cc \
$$XAPIANDIR/backends/chert/chert_spelling.cc \
$$XAPIANDIR/backends/chert/chert_spellingwordslist.cc \
$$XAPIANDIR/backends/chert/chert_synonym.cc \
$$XAPIANDIR/backends/chert/chert_table.cc \
$$XAPIANDIR/backends/chert/chert_termlist.cc \
$$XAPIANDIR/backends/chert/chert_termlisttable.cc \
$$XAPIANDIR/backends/chert/chert_valuelist.cc \
$$XAPIANDIR/backends/chert/chert_values.cc \
$$XAPIANDIR/backends/chert/chert_version.cc \
$$XAPIANDIR/backends/chert/chert_databasereplicator.cc \
$$XAPIANDIR/backends/glass/glass_alldocspostlist.cc \
$$XAPIANDIR/backends/glass/glass_alltermslist.cc \
$$XAPIANDIR/backends/glass/glass_changes.cc \
$$XAPIANDIR/backends/glass/glass_check.cc \
$$XAPIANDIR/backends/glass/glass_compact.cc \
$$XAPIANDIR/backends/glass/glass_cursor.cc \
$$XAPIANDIR/backends/glass/glass_database.cc \
$$XAPIANDIR/backends/glass/glass_dbcheck.cc \
$$XAPIANDIR/backends/glass/glass_document.cc \
$$XAPIANDIR/backends/glass/glass_freelist.cc \
$$XAPIANDIR/backends/glass/glass_inverter.cc \
$$XAPIANDIR/backends/glass/glass_metadata.cc \
$$XAPIANDIR/backends/glass/glass_positionlist.cc \
$$XAPIANDIR/backends/glass/glass_postlist.cc \
$$XAPIANDIR/backends/glass/glass_spelling.cc \
$$XAPIANDIR/backends/glass/glass_spellingwordslist.cc \
$$XAPIANDIR/backends/glass/glass_synonym.cc \
$$XAPIANDIR/backends/glass/glass_table.cc \
$$XAPIANDIR/backends/glass/glass_termlist.cc \
$$XAPIANDIR/backends/glass/glass_termlisttable.cc \
$$XAPIANDIR/backends/glass/glass_valuelist.cc \
$$XAPIANDIR/backends/glass/glass_values.cc \
$$XAPIANDIR/backends/glass/glass_version.cc \
$$XAPIANDIR/backends/glass/glass_databasereplicator.cc \
$$XAPIANDIR/backends/inmemory/inmemory_alltermslist.cc \
$$XAPIANDIR/backends/inmemory/inmemory_database.cc \
$$XAPIANDIR/backends/inmemory/inmemory_document.cc \
$$XAPIANDIR/backends/inmemory/inmemory_positionlist.cc \
$$XAPIANDIR/backends/multi/multi_alltermslist.cc \
$$XAPIANDIR/backends/multi/multi_postlist.cc \
$$XAPIANDIR/backends/multi/multi_termlist.cc \
$$XAPIANDIR/backends/multi/multi_valuelist.cc \
$$XAPIANDIR/backends/remote/remote-document.cc \
$$XAPIANDIR/backends/remote/net_postlist.cc \
$$XAPIANDIR/backends/remote/net_termlist.cc \
$$XAPIANDIR/backends/remote/remote-database.cc \
$$XAPIANDIR/common/bitstream.cc \
$$XAPIANDIR/common/closefrom.cc \
$$XAPIANDIR/common/debuglog.cc \
$$XAPIANDIR/common/errno_to_string.cc \
$$XAPIANDIR/common/fileutils.cc \
$$XAPIANDIR/common/io_utils.cc \
$$XAPIANDIR/common/keyword.cc \
$$XAPIANDIR/common/msvc_dirent.cc \
$$XAPIANDIR/common/omassert.cc \
$$XAPIANDIR/common/posixy_wrapper.cc \
$$XAPIANDIR/common/replicate_utils.cc \
$$XAPIANDIR/common/safe.cc \
$$XAPIANDIR/common/serialise-double.cc \
$$XAPIANDIR/common/socket_utils.cc \
$$XAPIANDIR/common/str.cc \
$$XAPIANDIR/common/compression_stream.cc \
$$XAPIANDIR/expand/bo1eweight.cc \
$$XAPIANDIR/expand/esetinternal.cc \
$$XAPIANDIR/expand/expandweight.cc \
$$XAPIANDIR/expand/ortermlist.cc \
$$XAPIANDIR/expand/tradeweight.cc \
$$XAPIANDIR/geospatial/geoencode.cc \
$$XAPIANDIR/geospatial/latlongcoord.cc \
$$XAPIANDIR/geospatial/latlong_distance_keymaker.cc \
$$XAPIANDIR/geospatial/latlong_metrics.cc \
$$XAPIANDIR/geospatial/latlong_posting_source.cc \
$$XAPIANDIR/languages/arabic.cc \
$$XAPIANDIR/languages/armenian.cc \
$$XAPIANDIR/languages/basque.cc \
$$XAPIANDIR/languages/catalan.cc \
$$XAPIANDIR/languages/danish.cc \
$$XAPIANDIR/languages/dutch.cc \
$$XAPIANDIR/languages/english.cc \
$$XAPIANDIR/languages/earlyenglish.cc \
$$XAPIANDIR/languages/finnish.cc \
$$XAPIANDIR/languages/french.cc \
$$XAPIANDIR/languages/german2.cc \
$$XAPIANDIR/languages/german.cc \
$$XAPIANDIR/languages/hungarian.cc \
$$XAPIANDIR/languages/indonesian.cc \
$$XAPIANDIR/languages/irish.cc \
$$XAPIANDIR/languages/italian.cc \
$$XAPIANDIR/languages/kraaij_pohlmann.cc \
$$XAPIANDIR/languages/lithuanian.cc \
$$XAPIANDIR/languages/lovins.cc \
$$XAPIANDIR/languages/nepali.cc \
$$XAPIANDIR/languages/norwegian.cc \
$$XAPIANDIR/languages/porter.cc \
$$XAPIANDIR/languages/portuguese.cc \
$$XAPIANDIR/languages/romanian.cc \
$$XAPIANDIR/languages/russian.cc \
$$XAPIANDIR/languages/spanish.cc \
$$XAPIANDIR/languages/swedish.cc \
$$XAPIANDIR/languages/tamil.cc \
$$XAPIANDIR/languages/turkish.cc \
$$XAPIANDIR/languages/stem.cc \
$$XAPIANDIR/languages/steminternal.cc \
$$XAPIANDIR/matcher/remotesubmatch.cc \
$$XAPIANDIR/matcher/andmaybepostlist.cc \
$$XAPIANDIR/matcher/andnotpostlist.cc \
$$XAPIANDIR/matcher/branchpostlist.cc \
$$XAPIANDIR/matcher/collapser.cc \
$$XAPIANDIR/matcher/exactphrasepostlist.cc \
$$XAPIANDIR/matcher/externalpostlist.cc \
$$XAPIANDIR/matcher/localsubmatch.cc \
$$XAPIANDIR/matcher/maxpostlist.cc \
$$XAPIANDIR/matcher/mergepostlist.cc \
$$XAPIANDIR/matcher/msetcmp.cc \
$$XAPIANDIR/matcher/msetpostlist.cc \
$$XAPIANDIR/matcher/multiandpostlist.cc \
$$XAPIANDIR/matcher/multimatch.cc \
$$XAPIANDIR/matcher/multixorpostlist.cc \
$$XAPIANDIR/matcher/nearpostlist.cc \
$$XAPIANDIR/matcher/orpositionlist.cc \
$$XAPIANDIR/matcher/orpospostlist.cc \
$$XAPIANDIR/matcher/orpostlist.cc \
$$XAPIANDIR/matcher/phrasepostlist.cc \
$$XAPIANDIR/matcher/selectpostlist.cc \
$$XAPIANDIR/matcher/synonympostlist.cc \
$$XAPIANDIR/matcher/valuegepostlist.cc \
$$XAPIANDIR/matcher/valuerangepostlist.cc \
$$XAPIANDIR/matcher/valuestreamdocument.cc \
$$XAPIANDIR/net/length.cc \
$$XAPIANDIR/net/serialise.cc \
$$XAPIANDIR/net/progclient.cc \
$$XAPIANDIR/net/remoteconnection.cc \
$$XAPIANDIR/net/remoteserver.cc \
$$XAPIANDIR/net/remotetcpclient.cc \
$$XAPIANDIR/net/remotetcpserver.cc \
$$XAPIANDIR/net/replicatetcpclient.cc \
$$XAPIANDIR/net/replicatetcpserver.cc \
$$XAPIANDIR/net/serialise-error.cc \
$$XAPIANDIR/net/tcpclient.cc \
$$XAPIANDIR/net/tcpserver.cc \
$$XAPIANDIR/queryparser/cjk-tokenizer.cc \
$$XAPIANDIR/queryparser/queryparser.cc \
$$XAPIANDIR/queryparser/queryparser_internal.cc \
$$XAPIANDIR/queryparser/termgenerator.cc \
$$XAPIANDIR/queryparser/termgenerator_internal.cc \
$$XAPIANDIR/unicode/description_append.cc \
$$XAPIANDIR/unicode/unicode-data.cc \
$$XAPIANDIR/unicode/utf8itor.cc \
$$XAPIANDIR/weight/bb2weight.cc \
$$XAPIANDIR/weight/bm25plusweight.cc \
$$XAPIANDIR/weight/bm25weight.cc \
$$XAPIANDIR/weight/boolweight.cc \
$$XAPIANDIR/weight/coordweight.cc \
$$XAPIANDIR/weight/dlhweight.cc \
$$XAPIANDIR/weight/dphweight.cc \
$$XAPIANDIR/weight/ifb2weight.cc \
$$XAPIANDIR/weight/ineb2weight.cc \
$$XAPIANDIR/weight/inl2weight.cc \
$$XAPIANDIR/weight/lmweight.cc \
$$XAPIANDIR/weight/pl2plusweight.cc \
$$XAPIANDIR/weight/pl2weight.cc \
$$XAPIANDIR/weight/tfidfweight.cc \
$$XAPIANDIR/weight/tradweight.cc \
$$XAPIANDIR/weight/weight.cc \
$$XAPIANDIR/weight/weightinternal.cc

INCLUDEPATH += $$XAPIANDIR $$XAPIANDIR/include \
           $$XAPIANDIR/common

windows {
  contains(QMAKE_CC, cl){
    # msvc
    CONFIG += staticlib
    DEFINES += USING_STATIC_LIBICONV
    INCLUDEPATH += \
      ../../../../recolldeps/msvc/zlib-1.2.11/ \
      ../../../../recolldeps/msvc/wlibiconv/include
   }
}
