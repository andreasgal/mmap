/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

#include <node.h>
#include <nan.h>

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

using v8::External;
using v8::FunctionTemplate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

using Nan::ErrnoException;
using Nan::False;
using Nan::New;
using Nan::NewBuffer;
using Nan::ThrowError;
using Nan::True;

struct hint_wrap {
	size_t length;
};

static void Map_finalise(char *data, void *hint_void) {
	hint_wrap *h = (hint_wrap *) hint_void;

	if (h->length > 0) {
		munmap(data, h->length);
	}

	delete h;
}

static NAN_METHOD(Sync) {
  Local<Value> self = info.This();
  if (!node::Buffer::HasInstance(self))
    return;
  char *data = node::Buffer::Data(self);
  size_t length = node::Buffer::Length(self);

	// First optional argument: offset
  if (info.Length() > 0 && info[0]->IsNumber()) {
		const size_t offset = size_t(info[0]->ToNumber()->Value());
		if (length <= offset)
      return;
		data += offset;
		length -= offset;
	}

	// Second optional argument: length
	if (info.Length() > 1 && info[1]->IsNumber()) {
		const size_t range = size_t(info[1]->ToNumber()->Value());
		if (range < length)
      length = range;
	}

	// Third optional argument: flags
	int flags;
	if (info.Length() > 2 && info[2]->IsNumber()) {
		flags = info[2]->ToInteger()->Value();
	} else {
		flags = MS_SYNC;
	}

	info.GetReturnValue().Set((0 == msync(data, length, flags)) ? True() : False());
}

NAN_METHOD(Unmap) {
  Local<Value> self = info.This();
  if (!node::Buffer::HasInstance(self))
    return;
  Local<Object> buffer = info.This()->ToObject();
  char *data = node::Buffer::Data(self);

	hint_wrap *d = (hint_wrap *) External::Cast(*buffer->GetHiddenValue(New("mmap_dptr").ToLocalChecked()))->Value();

	if (d->length > 0 && -1 == munmap(data, d->length)) {
    info.GetReturnValue().Set(False());
    return;
  }

  d->length = 0;
  v8::Uint8Array::Cast(*buffer)->Buffer()->Neuter();
  info.GetReturnValue().Set(True());
}

NAN_METHOD(Map) {
	if (info.Length() <= 3 || !info[0]->IsNumber() || !info[1]->IsNumber() || !info[2]->IsNumber() || !info[3]->IsNumber()) {
    ThrowError("mmap() takes 5 arguments: size, protection, flags, fd and offset.");
		return;
	}

	size_t length = size_t(info[0]->ToNumber()->Value());
	int protection = info[1]->ToInteger()->Value();
	int flags = info[2]->ToInteger()->Value();
	int fd = info[3]->ToInteger()->Value();

	off_t offset   = 0;
  if (info.Length() > 4 && info[4]->IsNumber())
    offset = off_t(info[4]->ToNumber()->Value());

	char* data = (char *) mmap(0, length, protection, flags, fd, offset);

	if (data == MAP_FAILED) {
    ErrnoException(errno, "mmap");
		return;
	}

  hint_wrap *d = new hint_wrap;
	d->length = length;

  Local<Object> buffer = NewBuffer(data, length, Map_finalise, (void *)d).ToLocalChecked();
  buffer->Set(New("unmap").ToLocalChecked(), New<FunctionTemplate>(Unmap)->GetFunction());
  buffer->Set(New("sync").ToLocalChecked(), New<FunctionTemplate>(Sync)->GetFunction());
  buffer->SetHiddenValue(New("mmap_dptr").ToLocalChecked(), New<v8::External>((void *)d));

  info.GetReturnValue().Set(buffer);
}


static void RegisterModule(Local<Object> exports)
{
	const int PAGESIZE = sysconf(_SC_PAGESIZE);

  SetMethod(exports, "map", Map);

	NODE_DEFINE_CONSTANT(exports, PROT_READ);
	NODE_DEFINE_CONSTANT(exports, PROT_WRITE);
	NODE_DEFINE_CONSTANT(exports, PROT_EXEC);
	NODE_DEFINE_CONSTANT(exports, PROT_NONE);
	NODE_DEFINE_CONSTANT(exports, MAP_SHARED);
	NODE_DEFINE_CONSTANT(exports, MAP_PRIVATE);
	NODE_DEFINE_CONSTANT(exports, PAGESIZE);
	NODE_DEFINE_CONSTANT(exports, MS_ASYNC);
	NODE_DEFINE_CONSTANT(exports, MS_SYNC);
	NODE_DEFINE_CONSTANT(exports, MS_INVALIDATE);
}

NODE_MODULE(mmap, RegisterModule);
