add_library (clickhouse-copier-lib ${LINK_MODE} ClusterCopier.cpp)
target_link_libraries (clickhouse-copier-lib PRIVATE clickhouse-server-lib clickhouse_functions clickhouse_aggregate_functions daemon)

if (CLICKHOUSE_SPLIT_BINARY)
    add_executable (clickhouse-copier clickhouse-copier.cpp)
    target_link_libraries (clickhouse-copier clickhouse-copier-lib)
    install (TARGETS clickhouse-copier ${CLICKHOUSE_ALL_TARGETS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT clickhouse)
endif ()
