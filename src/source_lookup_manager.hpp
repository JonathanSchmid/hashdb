// Author:  Bruce Allen <bdallen@nps.edu>
// Created: 2/25/2013
//
// The software provided here is released by the Naval Postgraduate
// School, an agency of the U.S. Department of Navy.  The software
// bears no warranty, either expressed or implied. NPS does not assume
// legal liability nor responsibility for a User's use of the software
// or the results of such use.
//
// Please note that within the United States, copyright protection,
// under Section 105 of the United States Code, Title 17, is not
// available for any work of the United States Government and/or for
// any works created by United States Government employees. User
// acknowledges that this software contains work which was created by
// NPS government employees and is therefore in the public domain and
// not subject to copyright.
//
// Released into the public domain on February 25, 2013 by Bruce Allen.

/**
 * \file
 * Provides interfaces for managing source lookup storage.
 */

#ifndef SOURCE_LOOKUP_STORE_HPP
#define SOURCE_LOOKUP_STORE_HPP
#include "hashdb_types.h"
#include "hashdb_settings.h"
#include "source_location_record.hpp"
#include "manager_multi_index_container.h"
#include "manager_bidirectional_btree.h"
#include <string>
#include <cassert>

/**
 * Provides interfaces for managing source lookup storage.
 * Source lookup storage is implemented using three bidirectional maps:
 *
 *   source_lookup_store:
 *       key=source_lookup_index,
 *       value=pair(repository_name_index, filename_index)
 *   repository_name_lookup_store:
 *       key=repository_name_index
 *       value=string_view
 *   filename_lookup_store:
 *       key=filename_index
 *       value=string_view
 */
class source_lookup_manager_t {
  private:
  const std::string hashdb_dir;
  const file_mode_type_t file_mode_type;
  uint64_t next_source_lookup_index;

  const bi_store_t<bi_64_pair_data_t> source_lookup_store;
  const bi_store_t<bi_64_sv_data_t> repository_name_lookup_store;
  const bi_store_t<bi_64_sv_data_t> filename_lookup_store;

  // disallow these
  source_lookup_manager_t(const source_lookup_manager_t&);
  source_lookup_manager_t& operator=(const source_lookup_manager_t&);

  public:
  source_lookup_manager_t (const std::string p_hashdb_dir,
                           file_mode_type_t p_file_mode_type) :
    hashdb_dir(p_hashdb_dir), file_mode_type(p_file_mode_type),
    next_source_lookup_index(0),
    source_lookup_store(hashdb_filenames_t::source_lookup_prefix(hashdb_dir),
              file_mode_type),
    repository_name_lookup_store(hashdb_filenames_t::source_repository_name_prefix(hashdb_dir),
              file_mode_type),
    filename_lookup_store(hashdb_filenames_t::source_filename_prefix(hashdb_dir)
              file_mode_type)
  {
    // if appending, find the next available source lookup index
    if (file_mode_type == RW_MODIFY && !source_lookup_store.empty()) {
      bi_store_t<bi_64_pair_data_t>::const_iterator it = source_lookup_store.rbegin();
      next_source_lookup_index = it->first + 1;
std::cout << "next source lookup index: " << next_source_lookup_index;
    }
  }

  /**
   * Close and release resources.
   */
  ~source_lookup_manager_t() {
  }

  /**
   * Get the source location of a source lookup index
   * else clear response and return false.
   */
  bool get_source_location(uint64_t source_lookup_index,
                           std::string& repository_name,
                           std::string& filename) {

    std::pair<uint64_t, uint64_t> lookup_pair;
    bool status1 = source_lookup_store.get_value(
                                     source_lookup_index, lookup_pair);
    if (status1 == false) {
      repository_name = "";
      filename = "";
      return false;
    } else {
      // get repository name from repository name index
      string_view repository_name_sv;
      bool status2 = repository_name_lookup_store.get_value(
                                      lookup_pair.first, repository_name_sv);
      if (status2 != true) {
        // program error
        assert(0);
      }
      repository_name = std::string(repository_name_sv);

      // get filename from filename index
      string_view filename_sv;
      bool status3 = filename_lookup_store.get_value(
                                      lookup_pair.first, filename_sv);
      if (status3 != true) {
        // program error
        assert(0);
      }
      filename = std::string(filename_sv);

      return true;
    }
  }

  /**
   * Get the source lookup index from source names
   * else clear response and return false.
   */
  bool get_source_lookup_index(const std::string& repository_name,
                               const std::string& filename,
                               uint64_t& source_lookup_index) {

    // get repository name index from repository name
    string_view repository_name_sv(repository_name);
    uint64_t repository_name_index;
    bool status1 = repository_name_lookup_store.get_key(
                                repository_name_sv, repository_name_index);
    if (status1 == false) {
      source_lookup_index = 0;
      return false;
    }

    // get filename index from filename
    string_view filename_sv(filename);
    uint64_t filename_index;
    bool status2 = filename_lookup_store.get_key(filename_sv, filename_index);
    if (status2 == false) {
      source_lookup_index = 0;
      return false;
    }

    // get source lookup index from looked up index values
    std::pair<uint64_t, uint64_t> index_pair(repository_name_index, filename_index);
    bool status3 = source_lookup_store.get_key(index_pair, source_lookup_index);
    if (status3 == false) {
      source_lookup_index = 0;
      return false;
    }

    return true;
  }

  /**
   * Insert the source location element else return false if already there.
   */
  bool insert_source_location_element(const std::string& repository_name,
                                      const std::string& filename,
                                      uint64_t& source_lookup_index) {

    // get or make repository name index from repository name
    string_view repository_name_sv(repository_name);
    uint64_t repository_name_index;
    bool status1 = repository_name_lookup_store.get_key(
                                repository_name_sv, repository_name_index);
    if (status1 == false) {
      // add new repository name using its new key
      repository_name_index = repository_name_lookup_store.insert_value(repository_name_sv);
    }

    // get or make filename index from filename
    string_view filename_sv(filename);
    uint64_t filename_index;
    bool status2 = filename_lookup_store.get_key(filename_sv, filename_index);
    if (status2 == false) {
      // add new repository name using its new key
      filename_index = filename_lookup_store.insert_value(filename_sv);
    }

    // define the index_pair value for the source lookup store
    std::pair index_pair(repository_name_index, filename_index);

    // look for existing key
    bool status3 = source_lookup_store.get_key(
                         index_pair, source_lookup_index);

    // either offer existing key or add element and offer new key
    if (status3 == false) {
      // insert the new element
      source_lookup_index = source_lookup_store.insert_value(index_pair);
      return true;
    } else {
      // just offer the index from the existing element
      return false;
    }
  }













  /**
   * Determine if a source location record exists.
   */
  bool has_source_location_record(
                  const source_location_record_t& source_location_record);

  /**
   * Insert else fail if left or right already exist.
   */
  void insert_source_lookup_element(uint64_t source_lookup_index,
                   const source_location_record_t& source_location_record);

  /**
   * Get the source location record from the source lookup index else fail.
   */
  void get_source_location_record(uint64_t source_lookup_index,
                     source_location_record_t& source_location_record);

  /**
   * Get the source lookup index from the source location record.
   */
  void get_source_lookup_index(
                const source_location_record_t& source_location_record,
                uint64_t& source_lookup_index);

  /**
   * Get the next unallocated source lookup index to use
   * or fail if they are all used up.
   */
  uint64_t new_source_lookup_index();

  /**
   * Report status to consumer.
   */
  template <class T>
  void report_status(T& consumer) const;
};

#endif
