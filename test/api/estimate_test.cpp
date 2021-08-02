#include <gtest/gtest.h>
#include <iostream>

#include <seqan3/test/expect_range_eq.hpp>

#include "ibf.h"
#include "shared.h"
#include "estimate.h"

#ifndef DATA_INPUT_DIR
#  define DATA_INPUT_DIR @DATA_INPUT_DIR@
#endif

using seqan3::operator""_shape;
std::filesystem::path tmp_dir = std::filesystem::temp_directory_path(); // get the temp directory

void initialization_args(estimate_ibf_arguments & args)
{
    args.compressed = true;
    args.k = 4;
    args.shape = seqan3::ungapped{args.k};
    args.w_size = seqan3::window_size{4};
    args.s = seqan3::seed{0};
    args.path_out = tmp_dir/"Test_";
    args.fpr = {0.05};
}

TEST(estimate, small_example)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    initialization_args(ibf_args);
    ibf_args.expression_levels = {1, 2, 4};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "mini_example.fasta"};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "mini_gen.fasta";
    estimate_args.path_in = ibf_args.path_out;

    ibf(sequence_files, ibf_args, minimiser_args);
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args);

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"gen1\t3\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
            EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_4");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, small_example_uncompressed)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    initialization_args(ibf_args);
    ibf_args.compressed = false;
    ibf_args.expression_levels = {1, 2, 4};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "mini_example.fasta"};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "mini_gen.fasta";
    estimate_args.path_in = ibf_args.path_out;

    ibf(sequence_files, ibf_args, minimiser_args);
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args);

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"gen1\t3\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
            EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_4");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, small_example_gene_not_found)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    initialization_args(ibf_args);
    ibf_args.expression_levels = {2, 4};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "mini_gen2.fasta";
    estimate_args.path_in = ibf_args.path_out;
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "mini_example.fasta"};

    ibf(sequence_files, ibf_args, minimiser_args);
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args);

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"gen2\t0\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
            EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_4");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, small_example_different_expressions_per_level_normalization_1)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    estimate_args.normalization_method = 1;
    initialization_args(ibf_args);
    ibf_args.number_expression_levels = 3;
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "mini_example.fasta"};

    minimiser(sequence_files, ibf_args, minimiser_args);
    std::vector<std::filesystem::path> minimiser_files{tmp_dir/"Test_mini_example.minimiser"};
    ibf_args.expression_levels = {};
    ibf(minimiser_files, ibf_args);

    ibf_args.expression_levels = {0, 1, 2};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "mini_gen.fasta";
    estimate_args.path_in = ibf_args.path_out;
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args, tmp_dir/"Test_IBF_Levels.levels");

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"gen1\t1\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
            EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_0");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_1");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_Levels.levels");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, example)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "exp_01.fasta", std::string(DATA_INPUT_DIR) + "exp_02.fasta",
                                                         std::string(DATA_INPUT_DIR) + "exp_11.fasta", std::string(DATA_INPUT_DIR) + "exp_12.fasta"};
    minimiser_args.samples = {2, 2};
    ibf_args.expression_levels = {32};
    ibf_args.fpr = {0.3};
    ibf_args.path_out = tmp_dir/"Test_";
    ibf_args.compressed = false;
    ibf(sequence_files, ibf_args, minimiser_args);

    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "gene.fasta";
    estimate_args.path_in = ibf_args.path_out;
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args);

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"GeneA\t0\t32\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
             EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_32");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, example_multiple_threads)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "exp_01.fasta", std::string(DATA_INPUT_DIR) + "exp_02.fasta",
                                                         std::string(DATA_INPUT_DIR) + "exp_11.fasta", std::string(DATA_INPUT_DIR) + "exp_12.fasta"};
    minimiser_args.samples = {2,2};
    ibf_args.expression_levels = {4, 32};
    ibf_args.fpr = {0.05};
    ibf_args.path_out = tmp_dir/"Test_";
    ibf_args.compressed = false;
    ibf(sequence_files, ibf_args, minimiser_args);
    ibf_args.threads = 4;

    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "gene.fasta";
    estimate_args.path_in = ibf_args.path_out;
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args);

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    std::string expected{"GeneA\t11\t32\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
             EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_32");
    std::filesystem::remove(tmp_dir/"Test_IBF_4");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, example_different_expressions_per_level)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "exp_01.fasta", std::string(DATA_INPUT_DIR) + "exp_02.fasta",
                                                         std::string(DATA_INPUT_DIR) + "exp_11.fasta", std::string(DATA_INPUT_DIR) + "exp_12.fasta"};
    minimiser_args.cutoffs = {0, 0};
    minimiser_args.samples = {2,2};
    ibf_args.number_expression_levels = 3;
    ibf_args.fpr = {0.05};
    ibf_args.path_out = tmp_dir/"Test_";
    ibf_args.compressed = false;
    minimiser(sequence_files, ibf_args, minimiser_args);
    std::vector<std::filesystem::path> minimiser_files{tmp_dir/"Test_exp_01.minimiser", tmp_dir/"Test_exp_11.minimiser"};
    ibf(minimiser_files, ibf_args);

    ibf_args.expression_levels = {0, 1, 2};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "gene.fasta";
    estimate_args.path_in = ibf_args.path_out;
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args, tmp_dir/"Test_IBF_Levels.levels");

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    // Count would expect 6 and 34
    std::string expected{"GeneA\t5\t26\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
             EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_0");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_1");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_Levels.levels");
    std::filesystem::remove(tmp_dir/"expression.out");
}

TEST(estimate, example_different_expressions_per_level_multiple_threads)
{
    estimate_ibf_arguments ibf_args{};
    minimiser_arguments minimiser_args{};
    estimate_arguments estimate_args{};
    std::vector<std::filesystem::path> sequence_files = {std::string(DATA_INPUT_DIR) + "exp_01.fasta", std::string(DATA_INPUT_DIR) + "exp_02.fasta",
                                                         std::string(DATA_INPUT_DIR) + "exp_11.fasta", std::string(DATA_INPUT_DIR) + "exp_12.fasta"};
    minimiser_args.cutoffs = {0, 0};
    minimiser_args.samples = {2,2};
    ibf_args.number_expression_levels = 3;
    ibf_args.fpr = {0.05};
    ibf_args.path_out = tmp_dir/"Test_";
    ibf_args.compressed = false;
    minimiser(sequence_files, ibf_args, minimiser_args);
    std::vector<std::filesystem::path> minimiser_files{tmp_dir/"Test_exp_01.minimiser", tmp_dir/"Test_exp_11.minimiser"};
    ibf_args.expression_levels = {};
    ibf(minimiser_files, ibf_args);

    ibf_args.threads = 4;
    ibf_args.expression_levels = {0, 1, 2};
    estimate_args.search_file = std::string(DATA_INPUT_DIR) + "gene.fasta";
    estimate_args.path_in = ibf_args.path_out;
    ibf_args.path_out = tmp_dir/"expression.out";
    call_estimate(ibf_args, estimate_args, tmp_dir/"Test_IBF_Levels.levels");

    std::ifstream output_file(tmp_dir/"expression.out");
    std::string line;
    // Count would expect 6 and 34
    std::string expected{"GeneA\t5\t26\t"};
    if (output_file.is_open())
    {
        while ( std::getline (output_file,line) )
        {
             EXPECT_EQ(expected,line);
        }
        output_file.close();
    }
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_0");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_1");
    std::filesystem::remove(tmp_dir/"Test_IBF_Level_2");
    std::filesystem::remove(tmp_dir/"Test_IBF_Levels.levels");
    std::filesystem::remove(tmp_dir/"expression.out");
}
