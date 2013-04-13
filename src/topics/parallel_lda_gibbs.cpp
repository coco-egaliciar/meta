/**
 * @file parallel_lda_gibbs.cpp
 */

#include "topics/parallel_lda_gibbs.h"

namespace meta {
namespace topics {

parallel_lda_gibbs::parallel_lda_gibbs( std::vector<index::Document> & docs, 
                                        size_t num_topics, double alpha, 
                                        double beta ) 
        : lda_gibbs( docs, num_topics, alpha, beta ) { }

parallel_lda_gibbs::~parallel_lda_gibbs() { /* nothing */ }

void parallel_lda_gibbs::initialize() {
    for( size_t i = 0; i < docs_.size(); ++i ) {
        doc_topic_count_[i] = {};
        doc_word_topic_[i] = {};
    }
    lda_gibbs::initialize();
}

void parallel_lda_gibbs::perform_iteration( bool init /* = false */ ) {
    size_t assigned = 0;
    auto range = util::range<size_t>( 0, docs_.size() - 1 );

    for( auto & id : pool_.thread_ids() ) {
        topic_term_diffs_[ id ] = {};
        topic_diffs_[ id ] = {};
    }

    parallel::parallel_for( range.begin(), range.end(), pool_, [&]( size_t i ) {
                {
                    std::lock_guard<std::mutex> lock( mutex_ );
                    common::show_progress( assigned++, docs_.size(), 10, "\t\t\t" );
                }
                size_t n = 0; // term number within document---constructed
                              // so that each occurrence of the same term
                              // can still be assigned a different topic
                for( const auto & freq : docs_[i].getFrequencies() ) {
                    for( size_t j = 0; j < freq.second; ++j ) {
                        size_t old_topic = doc_word_topic_[ i ][ n ];
                        // don't include current topic assignment in
                        // probability calculation
                        if( !init )
                            decrease_counts( old_topic, freq.first, i );

                        // sample a new topic assignment
                        size_t topic = sample_topic( freq.first, i );
                        doc_word_topic_[ i ][ n ] = topic;

                        // increase counts
                        increase_counts( topic, freq.first, i );
                        n += 1;
                    }
                }
            }
        );
    // perform reduction on the counts
    for( auto & thread_map : topic_term_diffs_ ) {
        for( auto & topic_term_map : thread_map.second ) {
            for( auto & diff : topic_term_map.second ) {
                topic_term_count_[ topic_term_map.first ][ diff.first ] += diff.second;
            }
            topic_count_[ topic_term_map.first ] += topic_diffs_[ thread_map.first ][ topic_term_map.first ];
        }
    }
}

void parallel_lda_gibbs::decrease_counts( size_t topic, TermID term, size_t doc ) {
    std::thread::id tid = std::this_thread::get_id();
    // decrease topic_term_diff_ for the given assignment
    topic_term_diffs_.at( tid )[ topic ][ term ] -= 1;

    // decrease doc_topic_count_ for the given assignment
    auto & dt_count = doc_topic_count_.at( doc ).at( topic );
    if( dt_count == 1 )
        doc_topic_count_.at( doc ).erase( topic );
    else
        dt_count -= 1;

    // decrease topic_diff
    topic_diffs_.at( tid )[ topic ] -= 1;
}

void parallel_lda_gibbs::increase_counts( size_t topic, TermID term, size_t doc ) {
    std::thread::id tid = std::this_thread::get_id();
    topic_term_diffs_.at( tid )[ topic ][ term ] += 1;
    doc_topic_count_[ doc ][ topic ] += 1;
    topic_diffs_.at( tid )[ topic ] += 1;
}

double parallel_lda_gibbs::count_term( TermID term, size_t topic ) const {
    double count = lda_gibbs::count_term( term, topic );
    std::thread::id tid = std::this_thread::get_id();
    if( topic_term_diffs_.find( tid ) == topic_term_diffs_.end() )
        return count;
    auto it = topic_term_diffs_.at( tid ).find( topic );
    if( it == topic_term_diffs_.at( tid ).end() )
        return count;
    auto iit = it->second.find( term );
    if( iit == it->second.end() )
        return count;
    return count + iit->second;
}

double parallel_lda_gibbs::count_topic( size_t topic ) const {
    double count = lda_gibbs::count_topic( topic );
    std::thread::id tid = std::this_thread::get_id();
    if( topic_diffs_.find( tid ) == topic_diffs_.end() )
        return count;
    auto it = topic_diffs_.at( tid ).find( topic );
    if( it == topic_diffs_.at( tid ).end() )
        return count;
    return count + it->second;
}

}
}
