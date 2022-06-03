#pragma once

#include <cstdint>

#include "choice.hpp"

struct queue_buffer_t;
queue_buffer_t* new_queue_buffer();

struct queue_consumer_t
{
  queue_buffer_t* buf;
  uint32_t pos;
  uint32_t meta;

  explicit queue_consumer_t(queue_buffer_t* buffer);
  queue_consumer_t(const queue_consumer_t&) = delete;
  queue_consumer_t& operator=(const queue_consumer_t&) = delete;

  void fetch();
  uint32_t size() const;
  choice_t get() const;
  bool next();
};

struct queue_producer_t
{
  queue_buffer_t* buf;
  uint32_t pos;

  explicit queue_producer_t(queue_buffer_t* buffer);
  queue_producer_t(const queue_producer_t&) = delete;
  queue_producer_t& operator=(const queue_producer_t&) = delete;

  void push(choice_t s);
  void stop();
};

// vim: sts=2 sw=2 et
