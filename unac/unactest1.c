/*
 * Copyright (C) 2000, 2001, 2002 Loic Dachary <loic@senga.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Run unac_string on an input large enough to trigger re-allocation.
 */ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "unac.h"

static char* longstr_expected = " 

Senga - Catalog software






   
    
 

    
  
   
     
       

  
  
   senga.org
   

  


   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  





December 28, 2000 
     
      January 27, 2000
      Catalog-1.02 
        is available. 
      
       The dmoz loading process has been dramatically simplified. It is
          now only available as a command. No more fancy web interface that
      confuses everyone. In addition the convert_dmoz script now generates
      text files that can be directly loaded into Catalog instead of the
      intermediate XML file. The whole loading process now takes from 
      one to two hours depending on your machine. It took around 10 hours
      with the previous version. 
       The -exclude option was added to convert_dmoz to get rid of 
          a whole branch of the catalog at load time. Typical usage would
      be convert_dmoz -exclude '^/Adult' -what content content.rdf.gz.
       A lot more sanity checks and repair have been added to deal with
          duplicates, category id conflicts and the like.
       Hopefully this new method will also be more understandable and 
          generate less traffic on the mailing list. There is room for 
      improvements and contributors are welcome. 
      
      A new set of software is available in the 
      download directory under the RedHat-6.1 section. These
      are the most up to date versions on which Catalog depends. Although the
      binaries depend on RedHat-6.1 the perl modules are source and can be
      used on any platform.
      
      September 7, 1999
      Catalog-1.01 
        is available. 
      This is a maintainance release.
      
        Various bug fixes. All easy
      to fix bugs have been fixed. Take a look at Bug Track to see what hasn't been fixed.
        The _PATHTEXT_ and _PATHFILE_ 
      tags syntax has been extended to specify a range of path component.
          
        Graham Barr added a recursive
      template feature for a catalog root page. This allows to show sub-categories
      of the root categories in the root page of a catalog.
          
      
      Don't hesitate to submit bugs
        or ideas to bug track. Hopefully the next version of Catalog will have
    a fast full text indexing mechanism and I'll be able to implement new
    functionalities.
        
      Have fun !
      July 3, 1999
      Catalog-1.00 
        is available. 
      This release includes PHP3 
        code to display a catalog. The author is Weston Bustraan (weston@infinityteldata.net). 
        The main motivation to jump directly to version 1.00 is to avoid version 
        number problems on CPAN. 
      July 2, 1999
      Catalog-0.19 
        is available. 
      This is a minor release. The 
        most noticeable addition is the new search mechanism.
      
        Searching : two search modes 
          are now available. AltaVista simple syntax and AltaVista advanced syntax. 
          Both use the Text-Query and Text-Query-SQL perl modules. 
        Dmoz loading is much more 
          fault tolerant. In addition it can handle compressed versions of content.rdf 
          and structure.rdf. The comments are now stored in text fields instead 
          of char(255).
        The template system was 
          extended with the pre_fill and post_fill parameters.
        Searching associated to 
          a catalog dumped to static pages is now possible using the 'static' 
          mode.
        Fixed two security weakness 
          in confedit and recursive cgi handling.
        Many sql queries have been 
          optimized.
        The configuration was changed 
          a bit to fix bugs and to isolate database dependencies.
        The tests were updated to 
          isolate database dependencies. 
        Fixed numerous minor bugs, 
          check ChangeLog if you're interested in details.
      
      Many thanks to Tim Bunce for 
        his numerous contributions and ideas. He is the architect of the Text-Query 
        and Text-Query-SQL modules, Eric Bohlman and Loic Dachary did the programming. 
        
      Thanks to Eric Bohlman for 
        his help on the Text-Query module. He was very busy but managed to spend 
        the time needed to release it. 
      There is not yet anything usable 
        for full text indexing but we keep working on it. The storage management 
        is now handled by the reiserfs file system thanks to Hans Reiser who is 
        working full time on this. Loic Dachary does his best to get something 
        working, if you're interested go to http://www.senga.org/mifluz/. 
      For some mysterious reason 
        CPAN lost track of Catalog name. In order to install catalog you should 
        use perl -MCPAN -e 'install Catalog::db'. Weird but temporary.
      Have fun !
       The Senga Team
        Ecila
        100 Av. du General Leclerc
        93 500 Pantin
        Tel: 33 1 56 96 09 80
        Fax: 33 1 56 96 09 81
        WEB: http://www.senga.org/
        Mail: senga@senga.org
      
    
  
  
     
    
      

[
Catalog |
webbase |
mifluz |
unac |
Search-Mifluz |
Text-Query |
uri |
Statistics |
News
]


    
  



";

static char* longstr = " 

Senga - Catalog software






   
    
 

    
  
   
     
       

  
  
   senga.org
   

  


   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  
   
    
  





December 28, 2000 
     
      January 27, 2000
      Catalog-1.02 
        is available. 
      
       The dmoz loading process has been dramatically simplified. It is
          now only available as a command. No more fancy web interface that
      confuses everyone. In addition the convert_dmoz script now generates
      text files that can be directly loaded into Catalog instead of the
      intermediate XML file. The whole loading process now takes from 
      one to two hours depending on your machine. It took around 10 hours
      with the previous version. 
       The -exclude option was added to convert_dmoz to get rid of 
          a whole branch of the catalog at load time. Typical usage would
      be convert_dmoz -exclude '^/Adult' -what content content.rdf.gz.
       A lot more sanity checks and repair have been added to deal with
          duplicates, category id conflicts and the like.
       Hopefully this new method will also be more understandable and 
          generate less traffic on the mailing list. There is room for 
      improvements and contributors are welcome. 
      
      A new set of software is available in the 
      download directory under the RedHat-6.1 section. These
      are the most up to date versions on which Catalog depends. Although the
      binaries depend on RedHat-6.1 the perl modules are source and can be
      used on any platform.
      
      September 7, 1999
      Catalog-1.01 
        is available. 
      This is a maintainance release.
      
        Various bug fixes. All easy
      to fix bugs have been fixed. Take a look at Bug Track to see what hasn't been fixed.
        The _PATHTEXT_ and _PATHFILE_ 
      tags syntax has been extended to specify a range of path component.
          
        Graham Barr added a recursive
      template feature for a catalog root page. This allows to show sub-categories
      of the root categories in the root page of a catalog.
          
      
      Don't hesitate to submit bugs
        or ideas to bug track. Hopefully the next version of Catalog will have
    a fast full text indexing mechanism and I'll be able to implement new
    functionalities.
        
      Have fun !
      July 3, 1999
      Catalog-1.00 
        is available. 
      This release includes PHP3 
        code to display a catalog. The author is Weston Bustraan (weston@infinityteldata.net). 
        The main motivation to jump directly to version 1.00 is to avoid version 
        number problems on CPAN. 
      July 2, 1999
      Catalog-0.19 
        is available. 
      This is a minor release. The 
        most noticeable addition is the new search mechanism.
      
        Searching : two search modes 
          are now available. AltaVista simple syntax and AltaVista advanced syntax. 
          Both use the Text-Query and Text-Query-SQL perl modules. 
        Dmoz loading is much more 
          fault tolerant. In addition it can handle compressed versions of content.rdf 
          and structure.rdf. The comments are now stored in text fields instead 
          of char(255).
        The template system was 
          extended with the pre_fill and post_fill parameters.
        Searching associated to 
          a catalog dumped to static pages is now possible using the 'static' 
          mode.
        Fixed two security weakness 
          in confedit and recursive cgi handling.
        Many sql queries have been 
          optimized.
        The configuration was changed 
          a bit to fix bugs and to isolate database dependencies.
        The tests were updated to 
          isolate database dependencies. 
        Fixed numerous minor bugs, 
          check ChangeLog if you're interested in details.
      
      Many thanks to Tim Bunce for 
        his numerous contributions and ideas. He is the architect of the Text-Query 
        and Text-Query-SQL modules, Eric Bohlman and Loic Dachary did the programming. 
        
      Thanks to Eric Bohlman for 
        his help on the Text-Query module. He was very busy but managed to spend 
        the time needed to release it. 
      There is not yet anything usable 
        for full text indexing but we keep working on it. The storage management 
        is now handled by the reiserfs file system thanks to Hans Reiser who is 
        working full time on this. Loic Dachary does his best to get something 
        working, if you're interested go to http://www.senga.org/mifluz/. 
      For some mysterious reason 
        CPAN lost track of Catalog name. In order to install catalog you should 
        use perl -MCPAN -e 'install Catalog::db'. Weird but temporary.
      Have fun !
       The Senga Team
        Ecila
        100 Av. du Général Leclerc
        93 500 Pantin
        Tel: 33 1 56 96 09 80
        Fax: 33 1 56 96 09 81
        WEB: http://www.senga.org/
        Mail: senga@senga.org
      
    
  
  
     
    
      

[
Catalog |
webbase |
mifluz |
unac |
Search-Mifluz |
Text-Query |
uri |
Statistics |
News
]


    
  



";

int main() {
  int i;
  char* out = 0;
  size_t out_length = 0;
  {
    if(unac_string("ISO-8859-1", "été", 3, &out, &out_length) < 0) {
      perror("unac été");
      exit(1);
    }
    if(out_length != 3) {
      fprintf(stderr, "out_length == %d instead of 3\n", (int)out_length);
      exit(1);
    }
    if(memcmp("ete", out, out_length)) {
      fprintf(stderr, "out == %.*s instead of ete\n", (int)out_length, out);
      exit(1);
    }

  }

  {
    char tmp[10];
    sprintf(tmp, "%c", 0xBC);
    if(unac_string("ISO-8859-1", tmp, 1, &out, &out_length) < 0) {
      perror("unac 0xBC (1/4)");
      exit(1);
    }
    if(out_length != 3) {
      fprintf(stderr, "out_length == %d instead of 3\n", (int)out_length);
      exit(1);
    }
    if(memcmp("1 4", out, out_length)) {
      fprintf(stderr, "out == %.*s instead of '1 4'\n", (int)out_length, out);
      exit(1);
    }

  }

  for(i = 0; i < 3; i++) {
    int longstr_length = strlen(longstr);
    if(unac_string("ISO-8859-1", longstr, longstr_length, &out, &out_length) == -1) {
      perror("unac_string longstr failed");
      exit(1);
    }
    if(out_length != longstr_length) {
      fprintf(stderr, "out_length == %d instead of %d\n", (int)out_length, longstr_length);
      exit(1);
    }
    if(memcmp(longstr_expected, out, out_length)) {
      fprintf(stderr, "out == %.*s instead of ete\n", (int)out_length, out);
      exit(1);
    }

  }

  free(out);

  return 0;
}
