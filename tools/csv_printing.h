/*
 * This file is part of the rc_dynamics_api package.
 *
 * Copyright (c) 2017 Roboception GmbH
 * All rights reserved
 *
 * Author: Christian Emmerich
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef RC_DYNAMICS_API_CSV_PRINTING_H_H
#define RC_DYNAMICS_API_CSV_PRINTING_H_H

#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <memory>

#include "rc_dynamics_api/msg_utils.h"

#include "roboception/msgs/frame.pb.h"
#include "roboception/msgs/dynamics.pb.h"
#include "roboception/msgs/imu.pb.h"

namespace csv {


/**
 * struct and methods to "organice" printing of csv-Headers
 */
struct Header {

  std::vector<std::string> _fields;
  std::string _prefix;

  static Header
  prefixed(const std::string &p, const ::google::protobuf::Message &m)
  {
    Header header;
    header._prefix = p;
    return header << m;
  }

  Header& operator <<(const std::string &field)
  {
    _fields.insert(std::end(_fields), _prefix + field);
    return *this;
  }

  Header& operator <<(const Header &other)
  {
    for (auto&& f : other._fields)
      _fields.insert(std::end(_fields), _prefix + f);
    return *this;
  }

  Header& operator <<(const ::google::protobuf::Message &m)
  {
    using namespace ::google::protobuf;

    auto descr = m.GetDescriptor();
    auto refl = m.GetReflection();
    for (int i=0; i<descr->field_count(); ++i)
    {
      auto field = descr->field(i);

      if (field->is_repeated())
      {
        int size = refl->FieldSize(m, field);
        if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE)
        {
          for (int i=0; i<size; i++)
          {
            *this << prefixed(field->name() + '_' + std::to_string(i),
                              refl->GetMessage(m, field));
          }
        } else {
          for (int i=0; i<size; i++)
          {
            *this << field->name() + "_" + std::to_string(i);
          }
        }
      }
      else if (!field->is_optional() || refl->HasField(m, field))
      {
        if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE)
        {
          *this << prefixed(field->name() + '_', refl->GetMessage(m, field));
        } else {
          *this << field->name();
        }
      }

    }

    return *this;
  }

};


/**
 * struct and methods to "organice" printing of csv-Lines
 */
struct Line
{
  std::vector<std::string> _entries;


  Line& operator <<(const std::string &t)
  {
    this->_entries.insert(std::end(this->_entries), t);
    return *this;
  }

  Line &operator<<(const ::google::protobuf::Message &m)
  {
    using namespace ::google::protobuf;

    auto descr = m.GetDescriptor();
    auto refl = m.GetReflection();
    for (int i=0; i<descr->field_count(); ++i)
    {
      auto field = descr->field(i);

      if (field->is_repeated())
      {
        int size = refl->FieldSize(m, field);
        switch (field->cpp_type()) {
          case FieldDescriptor::CPPTYPE_MESSAGE:
            for (int i=0; i<size;++i)
            {
              *this << refl->GetRepeatedMessage(m, field, i);
            }
            break;
          case FieldDescriptor::CPPTYPE_BOOL:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedBool(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_ENUM:
            for (int i=0; i<size;++i)
            {
              *this << refl->GetRepeatedEnum(m, field, i)->name();
            }
            break;
          case FieldDescriptor::CPPTYPE_FLOAT:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedFloat(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_DOUBLE:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedDouble(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_UINT32:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedUInt32(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_UINT64:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedUInt64(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_INT32:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedInt32(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_INT64:
            for (int i=0; i<size;++i)
            {
              *this << std::to_string(refl->GetRepeatedInt64(m, field, i));
            }
            break;
          case FieldDescriptor::CPPTYPE_STRING:
            for (int i=0; i<size;++i)
            {
              *this << refl->GetRepeatedString(m, field, i);
            }
            break;
        }

      } else if (!field->is_optional() || refl->HasField(m, field))
      {
        switch (field->cpp_type()) {
          case FieldDescriptor::CPPTYPE_MESSAGE:
            *this << refl->GetMessage(m, field);
            break;
          case FieldDescriptor::CPPTYPE_BOOL:
            *this << std::to_string(refl->GetBool(m, field));
            break;
          case FieldDescriptor::CPPTYPE_ENUM:
            *this << refl->GetEnum(m, field)->name();
            break;
          case FieldDescriptor::CPPTYPE_FLOAT:
            *this << std::to_string(refl->GetFloat(m, field));
            break;
          case FieldDescriptor::CPPTYPE_DOUBLE:
            *this << std::to_string(refl->GetDouble(m, field));
            break;
          case FieldDescriptor::CPPTYPE_UINT32:
            *this << std::to_string(refl->GetUInt32(m, field));
            break;
          case FieldDescriptor::CPPTYPE_UINT64:
            *this << std::to_string(refl->GetUInt64(m, field));
            break;
          case FieldDescriptor::CPPTYPE_INT32:
            *this << std::to_string(refl->GetInt32(m, field));
            break;
          case FieldDescriptor::CPPTYPE_INT64:
            *this << std::to_string(refl->GetInt64(m, field));
            break;
          case FieldDescriptor::CPPTYPE_STRING:
            *this << refl->GetString(m, field);
            break;
        }
      }
    }

    return *this;
  }
};

}



std::ostream& operator <<(std::ostream& s, const csv::Header &header)
{
  bool first = true;
  for (auto&& hf : header._fields)
  {
    if (first)
      s << hf;
    else
      s << "," << hf;
    first = false;
  }
  return s;
}



std::ostream& operator<<(std::ostream& s, const csv::Line &csv)
{
  bool first = true;
  for (auto&& e : csv._entries)
  {
    if (first)
    {
      s << e;
    } else {
      s << "," << e;
    }
    first = false;
  }

  return s;
}

#endif //RC_DYNAMICS_API_CSV_PRINTING_H_H
