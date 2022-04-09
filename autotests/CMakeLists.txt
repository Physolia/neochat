# SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
# SPDX-License-Identifier: BSD-2-Clause

enable_testing()

ecm_add_test(
    neochatroomtest.cpp
    LINK_LIBRARIES neochatlib Qt::Test
    TEST_NAME neochatroomtest
)
