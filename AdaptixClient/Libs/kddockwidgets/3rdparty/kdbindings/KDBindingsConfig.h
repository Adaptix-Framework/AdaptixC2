/*
  This file is part of KDBindings.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#ifdef KDBINDINGS_ENABLE_WARN_UNUSED
#define KDBINDINGS_WARN_UNUSED [[nodiscard]]
#else
#define KDBINDINGS_WARN_UNUSED
#endif
