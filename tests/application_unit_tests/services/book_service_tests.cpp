#include <gtest/gtest.h>

#include "book_service.hpp"

using application::services::BookService;

TEST(BookService, GettersAreSafeWithoutAnOpenBook)
{
    BookService service;

    EXPECT_TRUE(service.getFilePath().isEmpty());
    EXPECT_EQ(service.getPageCount(), 0);
    EXPECT_EQ(service.getCurrentPage(), 0);
    EXPECT_EQ(service.getFzDocument(), nullptr);
}
