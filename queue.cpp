#include "queue.hpp"

#include <cstddef>
#include <atomic>

static constexpr size_t BUFSIZE = 4096 - 2;
static constexpr uint32_t MASKSIZE = 0x3FFFFFFF;
static constexpr uint32_t MASKSTOP = 0x80000000;
static constexpr uint32_t MASKNEXT = 0x40000000;


struct queue_buffer_t
{
  const char* data[BUFSIZE] {0};
  queue_buffer_t* next {nullptr};
  std::atomic<uint32_t> meta {0};
};

queue_buffer_t* new_queue_buffer()
{
  return new queue_buffer_t();
}


uint32_t queue_consumer_t::size() const
{
  return meta & MASKSIZE;
}

void queue_consumer_t::load()
{
  meta = buf->meta;
}

const char* queue_consumer_t::get() const
{
  return buf->data[pos];
}

bool queue_consumer_t::next()
{
  if (meta & MASKNEXT) {
    auto nbuf = buf->next;
    delete buf;
    buf = nbuf;
    pos = 0;
  } else if (meta & MASKSTOP) {
    delete buf;
    buf = nullptr;
    pos = 0;
    meta = 0;
    return false;
  }
  return true;
}


void queue_producer_t::push(const char* s)
{
  if (pos < BUFSIZE) {
    buf->data[pos++] = s;
    buf->meta = pos;
  } else {
    auto nbuf = new queue_buffer_t;
    nbuf->data[0] = s;
    nbuf->meta = 1;
    buf->next = nbuf;
    buf->meta = BUFSIZE | MASKNEXT;
    buf = nbuf;
    pos = 1;
  }
}

void queue_producer_t::stop()
{
  buf->meta |= MASKSTOP;
}

// vim: sts=2 sw=2 et
