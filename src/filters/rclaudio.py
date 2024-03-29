#!/usr/bin/env python3

# Audio tag extractor for Recoll, using mutagen

import sys
import os
import rclexecm
from rclbasehandler import RclBaseHandler
import time
import datetime
import re

import rclconfig

try:
    import mutagen
    from mutagen import File
    from mutagen.id3 import ID3, ID3TimeStamp
except:
    print("RECFILTERROR HELPERNOTFOUND python3:mutagen")
    sys.exit(1);


re_pairnum = re.compile(b'''[([]*([0-9]+),\s*([0-9]+)''')

# The 'Easy' mutagen tags conversions are incomplete. We do it ourselves.
# TPA,TPOS,disc DISCNUMBER/TOTALDISCS
# TRCK,TRK,trkn TRACKNUMBER/TOTALTRACKS
# The conversions here are consistent with the ones in MinimServer (2019-03),
# including the rating stuff and TXXX. Lacking: Itunes '----' handling ?

# The 'GROUP' tag is a specific minimserver tag used to create
# sub-containers inside a folder. We used to use 'CONTENTGROUP' for
# this, which was wrong, the latter is a vaguely defined "music
# category" thing.
tagdict = {
    'ALBUM ARTIST': 'ALBUMARTIST',
    'ALBUM' : 'ALBUM',
    'ALBUMARTIST' : 'ALBUMARTIST',
    'ALBUMARTISTSORT' : 'ALBUMARTISTSORT',
    'ALBUMSORT' : 'ALBUMSORT',
    'ARTIST' : 'ARTIST',
    'ARTISTSORT' : 'ARTISTSORT',
    'BPM' : 'BPM',
    'COM' : 'COMMENT',
    'COMM' : 'COMMENT',
    'COMMENT' : 'COMMENT',
    'COMPILATION' : 'COMPILATION',
    'COMPOSER' : 'COMPOSER',
    'COMPOSERSORT' : 'COMPOSERSORT',
    'CONDUCTOR' : 'CONDUCTOR',
    'CONTENTGROUP' : 'CONTENTGROUP',
    'COPYRIGHT' : 'COPYRIGHT',
    'DATE' : 'DATE',
    'DISCNUMBER' : 'DISCNUMBER',
    'DISCSUBTITLE' : 'DISCSUBTITLE',
    'DISCTOTAL' : 'TOTALDISCS',
    'ENCODEDBY' : 'ENCODEDBY',
    'ENSEMBLE' : 'ORCHESTRA',
    'GENRE' : 'GENRE',
    'GROUP' : 'GROUP',
    'ISRC' : 'ISRC',
    'LABEL' : 'LABEL',
    'LANGUAGE' : 'LANGUAGE',
    'LYRICIST' : 'LYRICIST',
    'LYRICS' : 'LYRICS',
    'MOOD' : 'MOOD',
    'ORCHESTRA' : 'ORCHESTRA',
    'PERFORMER' : 'PERFORMER',
    'POP' : 'RATING1',
    'POPM' : 'RATING1',
    'ORIGINALARTIST' : 'ORIGINALARTIST',
    'ORIGINALDATE' : 'ORIGINALDATE',
    'RELEASEDATE' : 'RELEASEDATE',
    'REMIXER' : 'REMIXER',
    'SUBTITLE' : 'SUBTITLE',
    'TAL' : 'ALBUM',
    'TALB' : 'ALBUM',
    'TBP' : 'BPM',
    'TBPM' : 'BPM',
    'TCM' : 'COMPOSER',
    'TCMP' : 'COMPILATION',
    'TCO' : 'GENRE',
    'TCOM' : 'COMPOSER',
    'TCON' : 'GENRE',
    'TCOP' : 'COPYRIGHT',
    'TCP' : 'COMPILATION',
    'TCR' : 'COPYRIGHT',
    'TDA' : 'DATE',
    'TDAT' : 'DATE',
    'TDOR' : 'ORIGINALDATE',
    'TDRC' : 'DATE',
    'TDRL' : 'RELEASEDATE',
    'TEN' : 'ENCODEDBY',
    'TENC' : 'ENCODEDBY',
    'TEXT' : 'LYRICIST',
    'TIT1' : 'CONTENTGROUP',
    'TIT2' : 'TITLE',
    'TIT3' : 'SUBTITLE',
    'TITLE' : 'TITLE',
    'TITLESORT' : 'TITLESORT',
    'TLA' : 'LANGUAGE',
    'TLAN' : 'LANGUAGE',
    'TMOO' : 'MOOD',
    'TOA' : 'ORIGINALARTIST',
    'TOPE' : 'ORIGINALARTIST',
    'TOR' : 'ORIGINALDATE',
    'TORY' : 'ORIGINALDATE',
    'TOTALDISCS' : 'TOTALDISCS',
    'TOTALTRACKS' : 'TOTALTRACKS',
    'TP1' : 'ARTIST',
    'TP2' : 'ALBUMARTIST',
    'TP3' : 'CONDUCTOR',
    'TP4' : 'REMIXER',
    'TPA' : 'DISCNUMBER',
    'TPB' : 'LABEL',
    'TPE1' : 'ARTIST',
    'TPE2' : 'ALBUMARTIST',
    'TPE3' : 'CONDUCTOR',
    'TPE4' : 'REMIXER',
    'TPOS' : 'DISCNUMBER',
    'TPUB' : 'LABEL',
    'TRACK' : 'TRACKNUMBER',
    'TRACKNUM' : 'TRACKNUMBER',
    'TRACKNUMBER' : 'TRACKNUMBER',
    'TRACKTOTAL' : 'TOTALTRACKS',
    'TRC' : 'ISRC',
    'TRCK' : 'TRACKNUMBER',
    'TRDA' : 'DATE',
    'TRK' : 'TRACKNUMBER',
    'TS2' : 'ALBUMARTISTSORT',
    'TSA' : 'ALBUMSORT',
    'TSC' : 'COMPOSERSORT',
    'TSO2' : 'ALBUMARTISTSORT',
    'TSOA' : 'ALBUMSORT',
    'TSOC' : 'COMPOSERSORT',
    'TSOP' : 'ARTISTSORT',
    'TSOT' : 'TITLESORT',
    'TSP' : 'ARTISTSORT',
    'TSRC' : 'ISRC',
    'TSST' : 'DISCSUBTITLE',
    'TST' : 'TITLESORT',
    'TT1' : 'CONTENTGROUP',
    'TT2' : 'TITLE',
    'TT3' : 'SUBTITLE',
    'TXT' : 'LYRICIST',
    'TXXX:ORCHESTRA' : 'ORCHESTRA',
    'TXX:ORCHESTRA' : 'ORCHESTRA',
    'TYE' : 'DATE',
    'TYER' : 'DATE',# wikipedia id3: YEAR
    'ULT' : 'LYRICS',
    'USLT' : 'LYRICS',
    'YEAR' : 'DATE',
    'aART' : 'ALBUMARTIST',
    'cond' : 'CONDUCTOR',
    'cpil' : 'COMPILATION',
    'cprt' : 'COPYRIGHT',
    'disk' : 'DISCNUMBER',
    'gnre' : 'GENRE',
    'labl' : 'LABEL',
    'soaa' : 'ALBUMARTISTSORT',
    'soal' : 'ALBUMSORT',
    'soar' : 'ARTISTSORT',
    'soco' : 'COMPOSERSORT',
    'sonm' : 'TITLESORT',
    'tmpo' : 'BPM',
    'trkn' : 'TRACKNUMBER',
    '\xa9ART' : 'ARTIST',
    '\xa9alb' : 'ALBUM',
    '\xa9cmt' : 'COMMENT',
    '\xa9con' : 'CONDUCTOR',
    '\xa9day' : 'DATE',
    '\xa9gen' : 'GENRE',
    '\xa9grp' : 'CONTENTGROUP',
    '\xa9lyr' : 'LYRICS',
    '\xa9nam' : 'TITLE',
    '\xa9ope' : 'ORIGINALARTIST',
    '\xa9too' : 'ENCODEDBY',
    '\xa9wrt' : 'COMPOSER',
    }

def tobytes(s):
    if type(s) == type(b''):
        return s
    if type(s) != type(u''):
        s = str(s)
    return s.encode('utf-8', errors='replace')
    
# mp3:      album, title, artist, genre, date, tracknumber
# flac:     album, title, artist, genre, xxx, tracknumber
# oggvorbis:album, title, artist, genre, date, tracknumber
class AudioTagExtractor(RclBaseHandler):

    def __init__(self, em):
        super(AudioTagExtractor, self).__init__(em)
        config = rclconfig.RclConfig()
        tagfixerfn = config.getConfParam("audiotagfixerscript")
        self.tagfix = None
        if tagfixerfn:
            import runpy
            try:
                d = runpy.run_path(tagfixerfn)
                self.tagfix = d['tagfix']
                self.tagfix()
            except Exception as ex:
                #self.em.rclog("tagfix script import failed: %s" % ex)
                pass
            
    def _showMutaInfo(self, mutf):
        self.em.rclog("%s" % mutf.info.pprint())
        for prop in dir(mutf.info):
            self.em.rclog("mutinfo: %s -> %s" %
                          (prop, getattr( mutf.info, prop)))


    def _fixrating(self, minf):
        if 'RATING1' in minf:
            if not 'RATING' in minf:
               val = int(minf['RATING1']) // 51 + 1
               if val > 5:
                   val = 5
               if val < 1:
                   val = 1
               minf['RATING'] = str(val)
            del minf['RATING1']


    def _embeddedImageFormat(self, mutf):
        #self.em.rclog("_embeddedImage: MIME: %s"%mutf.mime)
        try:
            # This fails if we're passed a mutagen.ID3 instead of File
            mime = mutf.mime
        except:
            return ''

        if 'audio/mp3' in mime:
            for tagname in mutf.keys():
                if tagname.startswith('APIC:'):
                    #self.em.rclog("mp3 img: %s" % mutf[tagname].mime)
                    return 'jpg' if mutf[tagname].mime == 'image/jpeg' else 'png'
        elif 'audio/flac' in mime:
            if mutf.pictures:
                return 'jpg' if mutf.pictures[0].mime == 'image/jpeg' else 'png'
        elif 'audio/mp4' in mime:
            if 'covr' in mutf.keys():
                format = mutf['covr'][0].imageformat 
                if format == mutagen.mp4.AtomDataType.JPEG:
                    return 'jpg'
                else:
                    return 'png'
        return ''

    # Date formats found in actual files (any date field): [1961] [1967-01-01]
    #  [1996-11-04T08:00:00Z] [] [0000]  [1994-08-08 07:00]
    # We don't try to process the time part.
    # The method translates the date into a Unix timestamp
    # which means possible trouble for pre-1970 recordings (negative time).
    # Oldest possible date with 32 bits time stamp is 1901, which is ok though.
    #
    # Previous recoll versions had an alias from date to dmtime, which
    # was wrong, because dmtime is the unix integer time. We have
    # removed the alias, and set dmtime from the parsed date value.
    def parsedate(self, dt):
        try:
            dt = dt.decode('utf-8', errors='ignore')
            if len(dt) > 10:
                dt = dt[0:10]
            l = dt.split('-')
            if len(l) > 3 or len(l) == 2 or len(l[0]) != 4 or l[0] == '0000':
                return ''
            if len(l) == 1:
                pdt = datetime.datetime.strptime(dt, "%Y")
            elif len(l) == 3:
                pdt = datetime.datetime.strptime(dt, "%Y-%m-%d")
            val = time.mktime(pdt.timetuple())
            return "%d" % val
        except:
            return 0


    def html_text(self, filename):
        if not self.inputmimetype:
            raise Exception("html_text: input MIME type not set")
        mimetype = self.inputmimetype

        # We actually output text/plain
        self.outputmimetype = 'text/plain'

        mutf = None
        msg = ''
        strex = ''
        try:
            mutf = File(filename)
        except Exception as ex:
            strex = str(ex)
            try:
                mutf = ID3(filename)
            except Exception as ex:
                strex += str(ex)

        if not mutf:
            # Note: mutagen will fail the open (and raise) for a valid
            # file with no tags. Maybe we should just return an empty
            # text in this case? We seem to get an empty str(ex) in
            # this case, and a non empty one for, e.g. permission
            # denied, but I am not sure that the emptiness will be
            # consistent for all file types. The point of detecting
            # this would be to avoid error messages and useless
            # retries.
            if not strex:
                return b''
            else:
                raise Exception("Open failed: %s" % strex)
        
        #self._showMutaInfo(mutf)

        ###################
        # Extract audio parameters. Not all file types supply all or
        # even use the same property names...
        # minf has natural str keys, and encoded values
        minf = {}
        for prop,dflt in [('sample_rate', 44100), ('channels', 2),
                          ('length', 0), ('bitrate', 0)]:
            try:
                minf[prop] = getattr(mutf.info, prop)
            except Exception as e:
                #self.em.rclog("NO %s prop: %s" % (prop, e))
                minf[prop] = dflt

        if minf['bitrate'] == 0 and minf['length'] > 0:
            br = int(os.path.getsize(filename)* 8 / minf['length'])
            minf['bitrate'] = br

        minf['duration'] = minf['length']
        del minf['length']

        # Bits/samp is named sample_size or bits_per_sample (depend on file tp)
        try:
            minf['bits_per_sample'] = getattr(mutf.info, 'bits_per_sample')
        except:
            try:
                minf['bits_per_sample'] = getattr(mutf.info, 'sample_size')
            except:
                #self.em.rclog("using default bits_per_sample")
                minf['bits_per_sample'] = 16

        for tag,val in minf.items():
            minf[tag] = tobytes(val)
            
        ####################
        # Metadata tags. The names vary depending on the file type. We
        # just have a big translation dictionary for all
        for tag,val in mutf.items():
            #print(f"TAG {tag} VAL {val}", file=sys.stderr)
            # Mutagen sends out COMM==eng= with tag COMM::eng We don't know what to do with the
            # language (or possible other attributes), so get rid of it for now:
            if tag.find("COMM::") == 0:
                tag = "COMM"
            if tag.find('TXXX:') == 0:
                tag = tag[5:].upper()
            elif tag.find('TXX:') == 0:
                tag = tag[4:].upper()
            elif tag.upper() in tagdict:
                tag = tag.upper()
            if tag in tagdict:
                #self.em.rclog("Original tag: <%s>, type0 %s val <%s>" %
                #              (tag, type(val), val))
                # Some file types return lists of value (e.g. FLAC)
                try:
                    val = " ".join(val)
                    #self.em.rclog("Joined tag: <%s>, type0 %s val <%s>" %
                    #              (tag, type(val), val))
                except:
                    pass
                ntag = tagdict[tag].lower()
                #self.em.rclog("New tag: %s" % ntag)
                try:
                    minf[ntag] = tobytes(val)
                    #self.em.rclog("Tag %s -> %s" % (ntag, val))
                except Exception as err:
                    self.em.rclog("Error while extracting tag: %s"%err)
            else:
                #self.em.rclog("Unprocessed tag: %s, value %s"%(tag,val))
                pass

        self._fixrating(minf)
        
        #self.em.rclog("minf after extract %s\n" % minf)

        # TPA,TPOS,disc DISCNUMBER/TOTALDISCS
        # TRCK,TRK,trkn TRACKNUMBER/TOTALTRACKS
        for what in ('disc', 'track'):
            k = what + 'number'
            if k in minf:
                l = minf[k]
                if not isinstance(l, tuple):
                    mo = re_pairnum.match(l)
                    if mo:
                        l = (mo.group(1), mo.group(2))
                    else:
                        l = l.split(b'/')
                else:
                    self.em.rclog("l is tuple: %s tp1 %s tp2 %S" %
                                  (l, type(l[0]), type(l[1])))
                if len(l) == 2:
                    minf[k] = l[0]
                    #self.em.rclog("minf[%s] = %s" % (k, minf[k]))
                    if l[1] != 0:
                        minf['total' + what + 's'] = l[1]
                #self.em.rclog("%s finally: %s" %(k,minf[k]))

        if 'orchestra' in minf:
            val = minf['orchestra']
            if val.startswith(b'orchestra='):
                minf['orchestra'] = val[10:]
                
        #self.em.rclog("minf after tags %s\n" % minf)

        # Check for embedded image. We just set a flag.
        embdimg = self._embeddedImageFormat(mutf)
        if embdimg:
            #self.em.rclog("Embedded image format: %s" % embdimg)
            minf['embdimg'] = tobytes(embdimg)
        
        self.em.setfield("charset", 'utf-8')
        if self.tagfix:
            self.tagfix(minf)

        if 'date' in minf:
            uxtime = self.parsedate(minf['date'])
            if uxtime:
                minf['dmtime'] = uxtime
                
        for tag,val in minf.items():
            #self.em.rclog("%s -> %s" % (tag, val))
            self.em.setfield(tag, val)
            # Compat with old version
            if tag == 'artist':
                self.em.setfield('author', val)    

        try:
            docdata = tobytes(mutf.pprint())
        except Exception as err:
            docdata = ""
            self.em.rclog("Doc pprint error: %s" % err)

        return docdata


def makeObject():
    print("makeObject");
    proto = rclexecm.RclExecM()
    print("makeObject: rclexecm ok");
    extract = AudioTagExtractor(proto)
    return 17


if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = AudioTagExtractor(proto)
    rclexecm.main(proto, extract)
