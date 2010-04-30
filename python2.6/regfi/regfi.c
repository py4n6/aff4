/*
** regfi.c
** 
** Made by (mic)
** Login   <mic@laptop>
** 
** Started on  Fri Apr 30 02:06:28 2010 mic
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#include "pyregfi.h"

static int RegistryFile_dest(void *self) {
  RegistryFile this = (RegistryFile)self;

  regfi_free(this->reg);
  close(this->fd);

  return 0;
};

static RegistryFile RegistryFile_Con(RegistryFile self, char *filename) {
  self->fd = open(filename, O_RDONLY);
  if(self->fd < 0) {
    RaiseError(EIOError, "Unable to open %s", filename);
    goto error;
  };

  self->reg = regfi_alloc(self->fd);

  if(!self->reg) {
    RaiseError(ERuntimeError, "REGFI Error: %s", regfi_log_get_str());
    goto error;
  };

  talloc_set_destructor((void *)self, RegistryFile_dest);
  return self;

 error:
  talloc_free(self);
  return NULL;
};

static KeyIterator RegistryFile_get_key(RegistryFile self, char **path, REGFI_ENCODING encoding) {
  return CONSTRUCT(KeyIterator, KeyIterator, Con, NULL, self, path, encoding);
};


VIRTUAL(RegistryFile, Object) {
  VMETHOD(Con) = RegistryFile_Con;
  VMETHOD(get_key) = RegistryFile_get_key;
} END_VIRTUAL

static int KeyIterator_dest(void *self) {
  KeyIterator this = (KeyIterator)self;

  regfi_iterator_free(this->iter);
  if(this->next_item) {
    regfi_free_record(this->next_item);
  };

  return 0;
};

static KeyIterator KeyIterator_Con(KeyIterator self, RegistryFile file, char **path,
                             REGFI_ENCODING encoding) {
  self->iter = regfi_iterator_new(file->reg, encoding);

  if(!self->iter) {
    RaiseError(ERuntimeError, "Error: %s", regfi_log_get_str());
    goto error;
  };

  talloc_set_destructor((void*)self, KeyIterator_dest);

  // Traverse to the path
  if(path[0]) {
    if(!regfi_iterator_walk_path(self->iter, (const char **)path)) {
      RaiseError(ERuntimeError, "Unable to walk down key path");
      goto error;
    };
  };

  // Get the first key in the list
  self->next_item = regfi_iterator_first_subkey(self->iter);

  return self;
 error:
  return NULL;
};

static void KeyIterator__iter__(KeyIterator self) {
  if(self->next_item) {
    regfi_free_record(self->next_item);
  };

  self->next_item = regfi_iterator_first_subkey(self->iter);
};

static REGFI_NK_REC *KeyIterator_next(KeyIterator self) {
  REGFI_NK_REC * result;

  if(!self->next_item) return NULL;

  result = self->next_item;

  self->next_item = regfi_iterator_next_subkey(self->iter);

  return result;
};

static ValueIterator KeyIterator_list_values(KeyIterator self) {
  return CONSTRUCT(ValueIterator, ValueIterator, Con, NULL, self);
};

VIRTUAL(KeyIterator, Object) {
  VMETHOD(Con) = KeyIterator_Con;
  VMETHOD(iternext) = KeyIterator_next;
  VMETHOD(__iter__) = KeyIterator__iter__;
  VMETHOD(list_values) = KeyIterator_list_values;
} END_VIRTUAL

static int ValueIterator_dest(void *self) {
  ValueIterator this = (ValueIterator)self;

  if(this->next_item) regfi_free_record(this->next_item);

  return 0;
};

static ValueIterator ValueIterator_Con(ValueIterator self, KeyIterator key) {
  // Take a copy of the iterator
  self->iter = key->iter;
  talloc_reference(self, self->iter);

  self->next_item = regfi_iterator_first_value(self->iter);

  talloc_set_destructor((void *)self, ValueIterator_dest);

  return self;
};

static void ValueIterator__iter__(ValueIterator self) {
  if(self->next_item) regfi_free_record(self->next_item);

  self->next_item = regfi_iterator_first_value(self->iter);
};

static RawData ValueIterator_iternext(ValueIterator self) {
  RawData result;
  REGFI_DATA *data;
  REGFI_VK_REC *rec = (REGFI_VK_REC *)self->next_item;

  if(!rec) return NULL;

  data = (REGFI_DATA *)regfi_iterator_fetch_data(self->iter, rec);
  if(!data) {
    RaiseError(ERuntimeError, "Unable to fetch data: %s", regfi_log_get_str());
    goto error;
  };

  switch(self->next_item->type) {
  case REG_EXPAND_SZ:
  case REG_SZ:
    result = (RawData)CONSTRUCT(DataString, RawData, Con, NULL, data, rec);
    break;

  case REG_DWORD:
    result = (RawData)CONSTRUCT(DWORDData, RawData, Con, NULL, data, rec);
    break;

  case REG_BINARY:
  default:
    result = (RawData)CONSTRUCT(RawData, RawData, Con, NULL, data, rec);
    break;
  };


  /*  if(self->next_item) {
    regfi_free_record(self->next_item);
  };
  */

  self->next_item = regfi_iterator_next_value(self->iter);

  return result;
 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(ValueIterator, Object) {
  VMETHOD(Con) = ValueIterator_Con;
  VMETHOD(__iter__) = ValueIterator__iter__;
  VMETHOD(iternext) = ValueIterator_iternext;
} END_VIRTUAL

static int RawData_dest(void *self) {
  RawData this = (RawData)self;

  if(this->data) {
    regfi_free_record(this->data);
  };

  if(this->rec) {
    regfi_free_record(this->rec);
  };

  return 0;
};

static RawData RawData_Con(RawData self, REGFI_DATA *data, REGFI_VK_REC *record) {
  self->rec = record;
  self->data = data;

  talloc_set_destructor((void *)self, RawData_dest);

  return self;
};

static int RawData_get_value(RawData self, char *buff, int len) {
  int available_to_read = min(len, self->data->interpreted_size);

  memcpy(buff, self->data->raw, available_to_read);

  return available_to_read;
};

VIRTUAL(RawData, Object) {
  VMETHOD(Con) = RawData_Con;
  VMETHOD(get_value) = RawData_get_value;
} END_VIRTUAL

static char *DataString_get_value(DataString self) {
  RawData this = (RawData)self;

  return this->data->interpreted.string;
};

VIRTUAL(DataString, RawData) {
  VMETHOD(get_value) = DataString_get_value;
} END_VIRTUAL

static uint64_t DWORDData_get_value(DWORDData self) {
  RawData this = (RawData)self;

  return this->data->interpreted.dword;
};

VIRTUAL(DWORDData, RawData) {
  VMETHOD(get_value) = DWORDData_get_value;
} END_VIRTUAL

void pyregfi_init() {
  INIT_CLASS(RegistryFile);

};
