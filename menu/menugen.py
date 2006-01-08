#!/usr/bin/env python

import sys, string

TEMPLATE_HEADER="""
/* -*- c -*- ------------------------------------------------------------- *
 *   
 *   Copyright 2004-2005 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef NULL
#define NULL ((void *) 0)
#endif

#include "menu.h"
#include "com32io.h"
#include <string.h>

int main(void)
{
  t_menuitem * curr;

  // Change the video mode here
  // setvideomode(0)

  // Set the title and window size
  
"""


TEMPLATE_FOOTER=""" 
  curr = showmenus(find_menu_num("main")); // Initial menu is the one called "main"

  if (curr)
  {
        if (curr->action == OPT_RUN)
        {
            if (issyslinux()) runsyslinuxcmd(curr->data);
            else csprint(curr->data,0x07);
            return 1;
        }
        csprint("Error in programming!",0x07);
  }
  return 0;
}
"""

class Menusystem:
   def __init__(self):
       self.menus = []
       self.types = {"run" : "OPT_RUN","exitmenu":"OPT_EXITMENU","submenu":"OPT_SUBMENU"}
       self.init_entry()
       self.init_menu()
       self.init_system()
       self.vtypes = string.join(self.types.keys()," OR ")
       self.vattrs = string.join(filter(lambda x: x[0] != "_", self.entry.keys())," OR ")
       self.mattrs = string.join(filter(lambda x: x[0] != "_", self.menu.keys())," OR ")
       self.itemtemplate = '  add_item("%(item)s","%(info)s",%(type)s,"%(data)s",0);\n'
       self.menutemplate = '\n  add_named_menu("%(name)s","%(title)s",-1);\n'
       self.systemplate = '\n  init_menusystem(%(title)s);\n  set_window_size(%(top)s,%(left)s,%(bot)s,%(right)s);\n'

   def init_entry(self):
       self.entry = { "item" : "",
                      "info" : "",
                      "data" : "",
                      "_updated" : None, # has this dictionary been updated
                      "type" : "run" }

   def init_menu(self):
       self.menu = {"title" : "",
                    "row" : "0xFF", # let system decide position
                    "col" : "0xFF", 
                    "_updated" : None,
                    "name" : "" }

   def init_system(self):
       self.system = { "title" : "",
                       "top" : "1", "left" : "1" , "bot" : "23", "right":"79" }

   def add_menu(self,name):
       self.add_item()
       self.init_menu()
       self.menu["name"] = name
       self.menu["_updated"] = 1
       self.menus.append( (self.menu,[]) )

   def add_item(self):
       if self.menu["_updated"]: # menu details have changed
          self.menus[-1][0].update(self.menu)
          self.init_menu()
       if self.entry["_updated"]:
          if not self.entry["info"]: 
             self.entry["info"] = self.entry["data"]
          if not self.menus:
             print "Error before line %d" % self.lineno
             print "REASON: menu must be declared before a menu item is declared"
             sys.exit(1)
          self.menus[-1][1].append(self.entry)
       self.init_entry()

   # remove quotes if reqd
   def rm_quote(self,str):
       str = str .strip()
       if (str[0] == str[-1]) and (str[0] in ['"',"'"]): # remove quotes
          str = str[1:-1]
       return str

   def set_item(self,name,value):
       if not self.entry.has_key(name):
          msg = ["Unknown attribute %s in line %d" % (name,self.lineno)]
          msg.append("REASON: Attribute must be one of %s" % self.vattrs)
          return string.join(msg,"\n")
       if name=="type" and not self.types.has_key(value):
          msg = [ "Unrecognized type %s in line %d" % (value,self.lineno)]
          msg.append("REASON: Valid types are %s" % self.vtypes)
          return string.join(msg,"\n")
       self.entry[name] = self.rm_quote(value)
       self.entry["_updated"] = 1
       return ""

   def set_menu(self,name,value):
       # no current menu yet. We are probably in global section
       if not self.menus: return "Error"
       if not self.menu.has_key(name):
          return "Error"
       self.menu[name] = self.rm_quote(value)
       self.menu["_updated"] = 1
       return ""

   def set_system(self,name,value):
       if not self.system.has_key(name):
          return "Error"
       self.system[name] = self.rm_quote(value)

   def set(self,name,value):
       msg = self.set_item(name,value)
       # if valid item attrbute done
       if not msg: return

       # check if attr is of menu
       err = self.set_menu(name,value)
       if not err: return
   
       # check if global attribute
       err = self.set_system(name,value)
       if not err: return

       # all errors so return item's error message
       print msg
       sys.exit(1)

   def print_entry(self,entry,fd):
       entry["type"] = self.types[entry["type"]]
       fd.write(self.itemtemplate % entry)

   def print_menu(self,menu,fd):
       if menu["name"] == "main": self.foundmain = 1
       fd.write(self.menutemplate % menu)
       if (menu["row"] != "0xFF") or (menu["col"] != "0xFF"):
          fd.write('  set_menu_pos(%(row)s,%(col)s);\n' % menu)
       
   # parts of C code which set attrs for entire menu system
   def print_system(self,fd):
       if self.system["title"]:
          self.system["title"] = '"%s"' % self.system["title"]
       else: self.system["title"] = "NULL"
       fd.write(self.systemplate % self.system)

   def output(self,filename):
       fd = open(filename,"w")
       self.foundmain = None
       fd.write(TEMPLATE_HEADER)
       self.print_system(fd)
       for (menu,items) in self.menus:
           self.print_menu(menu,fd)
           for entry in items: self.print_entry(entry,fd)
       fd.write(TEMPLATE_FOOTER)
       fd.close()
       if not self.foundmain:
          print "main menu not found"
          sys.exit(1)
       
   def input(self,filename):
       fd = open(filename,"r")
       self.lineno = 0
       for line in fd.readlines():
         self.lineno = self.lineno + 1
         if line and line[-1] in ["\r","\n"]: line = line[:-1]
         if line and line[-1] in ["\r","\n"]: line = line[:-1]
         if line and line[0] in ["#",";"]: continue

         try:
           # blank line -> starting a new entry
           if not line: 
              self.add_item()
              continue

           # starting a new section?
           if line[0] == "[" and line[-1] == "]":
              self.add_menu(line[1:-1])
              continue
           
           # add property of current entry
           pos = line.find("=") # find the first = in string
           if pos < 0:
              print "Syntax error in line %d" % self.lineno
              print "REASON: non-section lines must be of the form ATTRIBUTE=VALUE"
              sys.exit(1)
           attr = line[:pos].strip().lower()
           value = line[pos+1:].strip()
           self.set(attr,value)
         except:
            print "Error while parsing line %d: %s" % (self.lineno,line)
            raise
       fd.close()
       self.add_item()


def usage():
    print sys.argv[0]," inputfile outputfile"
    print "inputfile is the ini-like file declaring menu structure"
    print "outputfile is the C source which can be compiled"
    sys.exit(1)

def main():
    if len(sys.argv) <> 3: usage()
    inst = Menusystem()
    inst.input(sys.argv[1])
    inst.output(sys.argv[2])

if __name__ == "__main__":
   main()


