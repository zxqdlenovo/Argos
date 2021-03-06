//
//  block_chain.h
//  Argos
//
//  Created by Windoze on 12-7-9.
//  Copyright (c) 2012 0d0a.com. All rights reserved.
//

#include <stddef.h>
#include <stdint.h>
#include <iterator>
#include "common/argos_consts.h"
#include "common/mem_pool.h"

#ifndef Argos_block_list_h
#define Argos_block_list_h

namespace argos {
    namespace common {
        /**
         * Template block_chain use to store doc vector
         */
        template<typename T, int BLOCK_ELEMENTS=10>
        struct block_chain
        {
            // STL container compliance
            typedef T value_type;
            typedef block_chain<T> this_t;
            typedef size_t size_type;
            typedef value_type *pointer;
            typedef value_type &reference;
            typedef const value_type &const_reference;
            
            struct header {
                uint32_t size;
                uint32_t block_count;
                common::OFFSET head;
                common::OFFSET tail;
            };
            
            struct block {
                bool empty() const { return size==0; }
                bool full() const { return size==BLOCK_ELEMENTS; }
                void push_back(value_type v) {
                    content[size]=v;
                    size++;
                }
                uint32_t size;
                common::OFFSET next;
                value_type content[BLOCK_ELEMENTS];
            };
            
            typedef common::offptr_t<block> block_ptr_t;
            typedef common::offptr_t<header> header_ptr_t;
            
            struct iterator {
                typedef block_chain::value_type value_type;
                typedef block_chain::pointer pointer;
                typedef block_chain::reference reference;
                typedef block_chain::const_reference const_reference;
                typedef std::input_iterator_tag iterator_category;
                
                inline iterator()
                : pool_(0)
                , chain_(common::INVALID_OFFSET)
                , current_block_(common::INVALID_OFFSET)
                , idx_in_current_block_(0)
                , count_(0)
                , max_(0)
                {}
                
                inline iterator(const block_chain *chain)
                : pool_(chain->pool_)
                , chain_(chain->get_offset())
                , current_block_(chain->first_block().offset)
                , idx_in_current_block_(0)
                , count_(0)
                , max_(chain->size())
                {}
                
                inline value_type operator*() const {
                    return get();
                }
                
                inline void operator++() {
                    if (ended()) {
                        return;
                    }
                    
                    idx_in_current_block_++;
                    if (idx_in_current_block_==BLOCK_ELEMENTS) {
                        idx_in_current_block_=0;
                        current_block_=chain().next_block(current_block_);
                    }
                    count_++;
                    
                    if (count_>=max_) {
                        current_block_=common::INVALID_OFFSET;
                        return;
                    }
                }
                
                inline iterator &operator++(int) {
                    operator++();
                    return *this;
                }
                
                inline bool operator==(const iterator &other) const {
                    if (current_block_==common::INVALID_OFFSET && other.current_block_==common::INVALID_OFFSET) {
                        return true;
                    }
                    return (pool_==other.pool_)
                    && (chain_==other.chain_)
                    && (current_block_==other.current_block_)
                    && (idx_in_current_block_==other.idx_in_current_block_)
                    && (count_==other.count_);
                }
                
                inline bool operator!=(const iterator &other) const {
                    return !operator==(other);
                }
                
                inline operator bool() const {
                    return !ended();
                }
                
                inline bool ended() const {
                    return (!pool_) || (chain_==common::INVALID_OFFSET) || (current_block_==common::INVALID_OFFSET);
                }
                
                inline value_type get() const {
                    return block_ptr_t(chain().pool_, current_block_)->content[idx_in_current_block_];
                }
                
                this_t chain() const {
                    return this_t(pool_, chain_);
                }
                
                common::mem_pool *pool_;
                common::OFFSET chain_;
                common::OFFSET current_block_;
                int idx_in_current_block_;
                size_t count_;
                size_t max_;    // Remember size of chain, in case the chain is extended during iterating
            };
            
            /**
             * Constructor, create a new block chain in mem_pool
             */
            inline block_chain(common::mem_pool *pool)
            : pool_(pool)
            , header_(pool_->allocate(sizeof(struct header)))
            {}
            
            /**
             * Constructor, load an existing block chain
             */
            inline block_chain(common::mem_pool *pool, common::OFFSET off)
            : pool_(pool)
            , header_(off)
            {}

            /**
             * Return the base offset of this block chain in mem_pool
             */
            inline common::OFFSET get_offset() const {
                return header_;
            }
            
            /**
             * Return the number of elements contains
             */
            inline size_t size() const {
                if (block_count()==0) {
                    return 0;
                }
                return BLOCK_ELEMENTS * (block_count()-1) + last_block()->size;
            }
            
            /**
             * Return the number of elements can be contained without expansion
             */
            inline size_t capacity() const {
                return BLOCK_ELEMENTS * block_count();
            }
            
            /**
             * Return true if the chain is empty
             */
            inline bool empty() const {
                return size()==0;
            }
            
            /**
             * Return true if the chain is full, further push_back needs expansion
             */
            inline bool full() const {
                return size()==capacity();
            }
            
            /**
             * Add a new value at the tail, expand if needed
             */
            inline void push_back(value_type v) {
                if (full()) {
                    new_block();
                }
                last_block()->push_back(v);
            }
            
            /**
             * Iterator points to the first element
             */
            inline iterator begin() const {
                return iterator(this);
            }
            
            /**
             * Ended iterator
             */
            inline iterator end() const {
                return iterator();
            }
            
        private:
            inline header_ptr_t header() const {
                return header_ptr_t(pool_, header_);
            }
            
            inline size_t block_count() const {
                return header()->block_count;
            }
            
            inline block_ptr_t first_block() const
            {
                return block_ptr_t(pool_, header()->head);
            }
            
            inline block_ptr_t last_block() const
            {
                return block_ptr_t(pool_, header()->tail);
            }
            
            inline block_ptr_t next_block(block_ptr_t b) const
            {
                return block_ptr_t(pool_, b->next);
            }
            
            inline common::OFFSET next_block(common::OFFSET b) const
            {
                return block_ptr_t(pool_, b)->next;
            }
            
            inline block_ptr_t new_block()
            {
                block_ptr_t ret=pool_->allocate<block>();
                memset(ret.get(), 0xFF, sizeof(block));
                ret->size=0;
                ret->next=common::INVALID_OFFSET;
                if(block_count()==0)
                {
                    // This is the first block
                    header()->head=ret.offset;
                } else {
                    last_block()->next=ret.offset;
                }
                header()->tail=ret.offset;
                header()->block_count++;
                return ret;
            }
            
            common::mem_pool *pool_;
            common::OFFSET header_;
            friend class iterator;
        };
        
        typedef block_chain<docid> simple_doc_vector;
        
        struct doc_freq_pair {
            docid did;
            uint32_t freq;
            doc_freq_pair()
            : did(INVALID_DOC)
            , freq(0)
            {}
            
            doc_freq_pair(docid d)
            : did(d)
            , freq(1)
            {}

            doc_freq_pair(docid d, uint32_t f)
            : did(d)
            , freq(f)
            {}
            
            operator docid() const {
                return did;
            }
        };
        typedef block_chain<doc_freq_pair> doc_freq_vector;
    }   // End of namespace common
}   // End of namespace argos

#endif
