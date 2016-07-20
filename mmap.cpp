/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

#include <node.h>
#include <nan.h>

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

using v8::FunctionTemplate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;

using Nan::False;
using Nan::NewBuffer;
using Nan::True;

struct hint_wrap {
	size_t length;
};

static void Map_finalise(char *data, void *hint_void) {
	struct hint_wrap *h = (struct hint_wrap *) hint_void;

	if (h->length > 0) {
		munmap(data, h->length);
	}

	delete h;
}

static NAN_METHOD(Sync) {
  Local<Object> buffer = info.This()->ToObject();
	char *data = node::Buffer::Data(buffer);
	size_t length = node::Buffer::Length(buffer);

	// First optional argument: offset
  if (info.Length() > 0) {
		const size_t offset = info[0]->ToInteger()->Value();
		if (length <= offset)
      return;
		data += offset;
		length -= offset;
	}

	// Second optional argument: length
	if (info.Length() > 1) {
		const size_t range = info[1]->ToInteger()->Value();
		if (range < length)
      length = range;
	}

	// Third optional argument: flags
	int flags;
	if (info.Length() > 2) {
		flags = info[2]->ToInteger()->Value();
	} else {
		flags = MS_SYNC;
	}

	info.GetReturnValue().Set((0 == msync(data, length, flags)) ? Nan::True() : Nan::False());
}

NAN_METHOD(Unmap) {
	Local<Object> buffer = info.This()->ToObject();
	char *data = node::Buffer::Data(buffer);

	struct hint_wrap *d = (struct hint_wrap *) v8::External::Cast(*buffer->GetHiddenValue(Nan::New("mmap_dptr").ToLocalChecked()))->Value();

	bool ok = true;

	if (d->length > 0 && -1 == munmap(data, d->length)) {
		ok = false;
	} else {
		d->length = 0;
    buffer->Set(Nan::New("length").ToLocalChecked(), Nan::New<Number>(0));
	}

	info.GetReturnValue().Set(ok ? True() : False());
}

NAN_METHOD(Map) {
	if (info.Length() <= 3) {
    Nan::ThrowError("mmap() takes 4 arguments: size, protection, flags, fd and offset.");
		return;
	}

	const size_t length  = info[0]->ToInteger()->Value();
	const int protection = info[1]->ToInteger()->Value();
	const int flags      = info[2]->ToInteger()->Value();
	const int fd         = info[3]->ToInteger()->Value();
	const off_t offset   = info[4]->ToInteger()->Value();

	char* data = (char *) mmap(0, length, protection, flags, fd, offset);

	if (data == MAP_FAILED) {
    Nan::ErrnoException(errno, "mmap");
		return;
	}

	struct hint_wrap *d = new hint_wrap;
	d->length = length;

  Local<Object> buffer = NewBuffer(data, length, Map_finalise, (void *)d).ToLocalChecked();
  buffer->Set(Nan::New("unmap").ToLocalChecked(), Nan::New<FunctionTemplate>(Unmap)->GetFunction());
  buffer->Set(Nan::New("sync").ToLocalChecked(), Nan::New<FunctionTemplate>(Sync)->GetFunction());
  buffer->SetHiddenValue(Nan::New("mmap_dptr").ToLocalChecked(), Nan::New<v8::External>((void *)d));

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
