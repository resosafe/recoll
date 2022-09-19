#!/usr/bin/python3

from recoll import recoll

db  = recoll.connect()
q = db.query()
q.execute("orographic")

class HL:
    def startMatch(self, i):
        return "<span class='hit'>"
    def endMatch(self):
        return "</span>";

hlmeths = HL()

for doc in q:
    print("DOC %s" % doc.title)
    snippets = q.getsnippets(doc, maxoccs=-1, ctxwords=10, methods=hlmeths, sortbypage=False)
    print("Got %d snippets" % len(snippets))
    for snip in snippets:
        try:
            print("Page %d term [%s] snippet [%s]" % (snip[0], snip[1], snip[2]))
        except Exception as ex:
            print("Print failed: %s" % ex)
