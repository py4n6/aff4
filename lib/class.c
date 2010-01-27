/******************************************************
# Copyright 2004: Commonwealth of Australia.
#
# Developed by the Computer Network Vulnerability Team,
# Information Security Group.
# Department of Defence.
#
# Michael Cohen <scudette@users.sourceforge.net>
#
# ******************************************************
#  Version: FLAG  $Version: 0.87-pre1 Date: Thu Jun 12 00:48:38 EST 2008$
# ******************************************************
#
# * This program is free software; you can redistribute it and/or
# * modify it under the terms of the GNU General Public License
# * as published by the Free Software Foundation; either version 2
# * of the License, or (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# ******************************************************/
#include "class.h"

/** This is used for error reporting.
 */
#define BUFF_SIZE 1024

__thread char __error_str[BUFF_SIZE];
__thread enum _error_type _global_error;
__thread char *_traceback=NULL;

void *raise_errors(enum _error_type t, char *reason, ...) {
  if(reason) {
    va_list ap;

    va_start(ap, reason);

    vsnprintf(__error_str, BUFF_SIZE-1, reason,ap);
    __error_str[BUFF_SIZE-1]=0;

    _traceback = talloc_asprintf_append(_traceback, "%s", __error_str);
    va_end(ap);
  };

  _global_error = t;

  return NULL;
};

// Noone should instantiate Object directly. this should be already
// allocated therefore:

inline void Object_init(Object this) {
  this->__class__ = &__Object;
  this->__super__ = NULL;
};

struct Object_t __Object = {
  .__class__ = &__Object,
  .__super__ = &__Object,
  .__size = sizeof(struct Object_t)
};

int issubclass(Object obj, Object class) {
  obj = obj->__class__;
  while(1) {
    if(obj == class->__class__)
      return 1;

    obj=obj->__super__;

    if(obj == &__Object || obj==NULL) 
      return 0;
  };
};

void unimplemented(Object self) {
  printf("%s contains unimplemented functions.. is it an abstract class?\n", NAMEOF(self));
  abort();
};
