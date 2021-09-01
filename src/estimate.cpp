#include <deque>
#include <iostream>
#include <math.h>
#include <numeric>
#include <omp.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm> //reorded because of this error:https://github.com/Homebrew/homebrew-core/issues/44579

#include <seqan3/std/ranges>

#if SEQAN3_WITH_CEREAL
#include <cereal/archives/binary.hpp>
#endif // SEQAN3_WITH_CEREAL

#include <seqan3/alphabet/container/concatenated_sequences.hpp>
#include <seqan3/alphabet/nucleotide/dna4.hpp>
#include <seqan3/core/concept/cereal.hpp>
#include <seqan3/core/debug_stream.hpp>
#include <seqan3/io/sequence_file/all.hpp>

#include "estimate.h"
template <class IBFType, bool last_exp, bool normalization, typename exp_t>
void check_ibf(min_arguments const & args, IBFType const & ibf, std::vector<uint16_t> & estimations_i,
               seqan3::dna4_vector const seq, std::vector<uint32_t> & prev_counts,
               exp_t const & expressions, uint16_t const k, float const fpr)
{
    static constexpr bool multiple_expressions = std::same_as<exp_t, std::vector<std::vector<uint16_t>>>;

    std::vector<uint32_t> counter;
    counter.assign(ibf.bin_count(), 0);
    uint64_t minimiser_length = 0;
    for (auto minHash : seqan3::views::minimiser_hash(seq, args.shape, args.w_size, args.s))
    {
        auto agent = ibf.membership_agent();
        std::transform (counter.begin(), counter.end(), agent.bulk_contains(minHash).begin(), counter.begin(),
                        std::plus<int>());
        ++minimiser_length;
    }

    float minimiser_pos = minimiser_length/2.0;

    for(int j = 0; j < counter.size(); j++)
    {
        // Correction by substracting the expected number of false positives
        float seen = std::max((float) 0.0, (float) ((prev_counts[j]-(minimiser_length*fpr))/(1.0-fpr)));
        float seen2 = std::max((float) 0.0, (float) ((counter[j]-(minimiser_length*fpr))/(1.0-fpr)));

        if ((seen + seen2) >= minimiser_pos)
        {
            // If there was nothing previous
            if constexpr(last_exp)
            {
                if constexpr (multiple_expressions)
                    estimations_i[j] = expressions[k][j];
                else
                    estimations_i[j] = expressions;
            }
            else
            {
                if (seen2  == 0)
                    seen2++;

               // Actually calculate estimation, in the else case k stands for the prev_expression
               if constexpr (multiple_expressions)
                   estimations_i[j] = std::max(expressions[k][j] * 1.0, expressions[k+1][j] - ((abs(minimiser_pos - seen)/(seen2 * 1.0)) * (expressions[k+1][j]-expressions[k][j])));
               else
                   estimations_i[j] = std::max(expressions * 1.0, k - ((abs(minimiser_pos - seen)/(seen2 * 1.0)) * (k-expressions)));
               // Make sure, every transcript is only estimated once
               prev_counts[j] = 0;
            }

            if constexpr (normalization & multiple_expressions)
                estimations_i[j] = estimations_i[j]/expressions[0][j];
        }
        else
        {
            prev_counts[j] = prev_counts[j] + counter[j];
        }
    }


}

// Reads the level file ibf creates
void read_levels(std::vector<std::vector<uint16_t>> & expressions, std::filesystem::path filename)
{
    std::ifstream fin;
    fin.open(filename);
    auto stream_view = seqan3::views::istreambuf(fin);
    auto stream_it = std::ranges::begin(stream_view);
    int j{0};
    std::vector<uint16_t> empty_vector{};

    std::string buffer{};

    // Read line = expression levels
    do
    {
        if (j == expressions.size())
            expressions.push_back(empty_vector);
        std::ranges::copy(stream_view | seqan3::views::take_until_or_throw(seqan3::is_char<' '>),
                                        std::cpp20::back_inserter(buffer));
        expressions[j].push_back((uint16_t)  std::stoi(buffer));
        buffer.clear();
        if(*stream_it != '/')
            ++stream_it;

        if (*stream_it == '\n')
        {
            ++stream_it;
            j++;
        }
    } while (*stream_it != '/');
    ++stream_it;

    fin.close();
}

/*! \brief Function to estimate expression value.
*  \param args        The arguments.
*  \param ibf         The ibf determing what kind ibf is used (compressed or uncompressed).
*  \param file_out    The output file.
*  \param estimate_args  The estimate arguments.
*/
template <class IBFType, bool samplewise, bool normalization_method = false>
void estimate(estimate_ibf_arguments & args, IBFType & ibf, std::filesystem::path file_out,
              estimate_arguments const & estimate_args)
{
    std::vector<std::string> ids;
    std::vector<seqan3::dna4_vector> seqs;
    std::vector<uint32_t> counter;
    std::vector<uint16_t> counter_est;
    std::vector<std::vector<uint32_t>> prev_counts;
    uint64_t prev_expression;
    std::vector<std::vector<uint16_t>> estimations;
    std::vector<std::vector<uint16_t>> expressions;

    omp_set_num_threads(args.threads);

    seqan3::sequence_file_input<my_traits, seqan3::fields<seqan3::field::id, seqan3::field::seq>> fin{estimate_args.search_file};
    for (auto & [id, seq] : fin)
    {
        ids.push_back(id);
        seqs.push_back(seq);
    }

    if constexpr (samplewise)
        read_levels(expressions, estimate_args.path_in.string() + "IBF_Levels.levels");
    else
        prev_expression = 0;

    // Make sure expression levels are sorted.
    sort(args.expression_levels.begin(), args.expression_levels.end());

    // Initialse last expression
    if constexpr (samplewise)
        load_ibf(ibf, estimate_args.path_in.string() + "IBF_Level_" + std::to_string(args.number_expression_levels-1));
    else
        load_ibf(ibf, estimate_args.path_in.string() + "IBF_" + std::to_string(args.expression_levels[args.expression_levels.size()-1]));
    counter.assign(ibf.bin_count(), 0);
    counter_est.assign(ibf.bin_count(), 0);

    for (int i = 0; i < seqs.size(); ++i)
    {
        estimations.push_back(counter_est);
        prev_counts.push_back(counter);
    }
    counter_est.clear();
    counter.clear();

    // Go over the sequences
    #pragma omp parallel for
    for (int i = 0; i < seqs.size(); ++i)
    {
        if constexpr (samplewise & normalization_method)
            check_ibf<IBFType, true, true>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                           expressions,args.number_expression_levels - 1,
                                           args.fpr[args.number_expression_levels - 1]);
        else if constexpr (samplewise)
            check_ibf<IBFType, true, false>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                            expressions, args.number_expression_levels - 1,
                                            args.fpr[args.number_expression_levels - 1]);
        else
            check_ibf<IBFType, true, false>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                            args.expression_levels[args.expression_levels.size() - 1], prev_expression,
                                            args.fpr[args.expression_levels.size() - 1]);
    }

    if constexpr (!samplewise)
        prev_expression = args.expression_levels[args.expression_levels.size() - 1];

    for (int j = args.number_expression_levels - 2; j >= 0; j--)
    {
        if constexpr (samplewise)
            load_ibf(ibf, estimate_args.path_in.string() + "IBF_Level_" + std::to_string(j));
        else
            load_ibf(ibf, estimate_args.path_in.string() + "IBF_" + std::to_string(args.expression_levels[j]));

        // Go over the sequences
        #pragma omp parallel for
        for (int i = 0; i < seqs.size(); ++i)
        {
            if constexpr (samplewise & normalization_method)
                check_ibf<IBFType, false, true>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                      expressions, j, args.fpr[j]);
            else if constexpr (samplewise)
                check_ibf<IBFType, false, false>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                          expressions, j, args.fpr[j]);
            else
                check_ibf<IBFType, false, false>(args, ibf, estimations[i], seqs[i], prev_counts[i],
                                          args.expression_levels[j], prev_expression, args.fpr[j]);
        }

        if (!samplewise)
            prev_expression = args.expression_levels[j];
    }

    std::ofstream outfile;
    outfile.open(std::string{file_out});
    for (int i = 0; i <  seqs.size(); ++i)
    {
        outfile << ids[i] << "\t";
        for (int j = 0; j < ibf.bin_count(); ++j)
             outfile << estimations[i][j] << "\t";

        outfile << "\n";
    }
    outfile.close();

}

void call_estimate(estimate_ibf_arguments & args, estimate_arguments & estimate_args)
{
    load_args(args, std::string{estimate_args.path_in} + "IBF_Data");

    if (args.fpr.size() == 1)
    {
        args.fpr.assign(args.expression_levels.size(), args.fpr[0]);
    }

    if (args.compressed)
    {
        seqan3::interleaved_bloom_filter<seqan3::data_layout::compressed> ibf;
        if (args.samplewise)
        {
            if (estimate_args.normalization_method)
                estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::compressed>, true, true>(args, ibf, args.path_out, estimate_args);
            else
                estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::compressed>, true>(args, ibf, args.path_out, estimate_args);
        }
        else
        {
            estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::compressed>, false>(args, ibf, args.path_out, estimate_args);
        }
    }
    else
    {
        seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed> ibf;
        if (args.samplewise)
        {
            if (estimate_args.normalization_method)
                estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed>, true, true>(args, ibf, args.path_out, estimate_args);
            else
                estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed>, true>(args, ibf, args.path_out, estimate_args);
        }
        else
        {
            estimate<seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed>, false>(args, ibf, args.path_out, estimate_args);
        }
    }
}
