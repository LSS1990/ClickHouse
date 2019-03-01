#include <Columns/ColumnsNumber.h>

#include <DataTypes/DataTypesNumber.h>

#include <Processors/IProcessor.h>
#include <Processors/ISource.h>
#include <Processors/ISink.h>
#include <Processors/ISimpleTransform.h>
#include <Processors/LimitTransform.h>
#include <Processors/printPipeline.h>
#include <Processors/Transforms/MergeSortingTransform.h>
#include <Processors/Executors/PipelineExecutor.h>

#include <IO/WriteBufferFromFileDescriptor.h>
#include <IO/WriteBufferFromOStream.h>
#include <IO/WriteHelpers.h>

#include <Formats/FormatSettings.h>

#include <iostream>
#include <chrono>


using namespace DB;


class NumbersSource : public ISource
{
public:
    String getName() const override { return "Numbers"; }

    NumbersSource(UInt64 count, UInt64 block_size, unsigned sleep_useconds)
            : ISource(Block({ColumnWithTypeAndName{ ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number" }})),
            count(count), block_size(block_size), sleep_useconds(sleep_useconds)
    {
    }

private:
    UInt64 current_number = 0;
    UInt64 count;
    UInt64 block_size;
    unsigned sleep_useconds;

    Chunk generate() override
    {
        if (current_number == count)
            return {};

        usleep(sleep_useconds);

        MutableColumns columns;
        columns.emplace_back(ColumnUInt64::create());

        UInt64 number = current_number++;
        for (UInt64 i = 0; i < block_size; ++i, number += count)
            columns.back()->insert(Field(number));

        return Chunk(std::move(columns), block_size);
    }
};

class PrintSink : public ISink
{
public:
    String getName() const override { return "Print"; }

    PrintSink(String prefix)
            : ISink(Block({ColumnWithTypeAndName{ ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number" }})),
              prefix(std::move(prefix))
    {
    }

private:
    String prefix;
    WriteBufferFromFileDescriptor out{STDOUT_FILENO};
    FormatSettings settings;

    void consume(Chunk chunk) override
    {
        size_t rows = chunk.getNumRows();
        size_t columns = chunk.getNumColumns();

        for (size_t row_num = 0; row_num < rows; ++row_num)
        {
            writeString(prefix, out);
            for (size_t column_num = 0; column_num < columns; ++column_num)
            {
                if (column_num != 0)
                    writeChar('\t', out);
                getPort().getHeader().getByPosition(column_num).type->serializeAsText(*chunk.getColumns()[column_num], row_num, out, settings);
            }
            writeChar('\n', out);
        }

        out.next();
    }
};

template<typename TimeT = std::chrono::milliseconds>
struct measure
{
    template<typename F, typename ...Args>
    static typename TimeT::rep execution(F&& func, Args&&... args)
    {
        auto start = std::chrono::steady_clock::now();
        std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
        auto duration = std::chrono::duration_cast< TimeT>
                (std::chrono::steady_clock::now() - start);
        return duration.count();
    }
};

int main(int, char **)
try
{
    auto execute_chain = [](
        String msg,
        UInt64 source_block_size,
        UInt64 blocks_count,
        size_t max_merged_block_size,
        UInt64 limit,
        size_t max_bytes_before_remerge,
        size_t max_bytes_before_external_sort,
        ThreadPool * pool)
    {
        std::cerr << msg << "\n";

        auto source = std::make_shared<NumbersSource>(blocks_count, source_block_size, 100000);
        SortDescription description = {{0, 1, 1}};
        auto transform = std::make_shared<MergeSortingTransform>(
                source->getPort().getHeader(), description,
                max_merged_block_size, limit, max_bytes_before_remerge, max_bytes_before_external_sort, ".");
        auto sink = std::make_shared<PrintSink>("");

        connect(source->getPort(), transform->getInputs().front());
        connect(transform->getOutputs().front(), sink->getPort());

        std::vector<ProcessorPtr> processors = {source, transform, sink};
        WriteBufferFromOStream out(std::cout);
        printPipeline(processors, out);

        PipelineExecutor executor(processors, pool);
        executor.execute();
    };

    ThreadPool pool(4, 4, 10);
    std::vector<ThreadPool *> pools = {nullptr, &pool};
    std::map<std::string, Int64> times;

    for (auto pool : pools)
    {
        Int64 time = 0;

        UInt64 source_block_size = 100;
        UInt64 blocks_count = 100;
        size_t max_merged_block_size = 100;
        UInt64 limit = 0;
        size_t max_bytes_before_remerge = 10000000;
        size_t max_bytes_before_external_sort = 10000000;
        std::string msg = pool ? "multiple threads" : "single thread";
        msg += ", 100 blocks per 100 numbers, no remerge and external sorts.";

        time =  measure<>::execution(execute_chain, msg,
            source_block_size,
            blocks_count,
            max_merged_block_size,
            limit,
            max_bytes_before_remerge,
            max_bytes_before_external_sort,
            pool);

        times[msg] = time;
    }

    for (auto & item : times)
        std::cout << item.first << ' ' << item.second << " ms.\n";

    return 0;
}
catch (...)
{
    std::cerr << getCurrentExceptionMessage(true) << '\n';
    throw;
}