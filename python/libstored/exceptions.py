# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

class Disconnected(RuntimeError):
    pass

class OperationFailed(RuntimeError):
    pass

class InvalidState(RuntimeError):
    pass

class NotSupported(RuntimeError):
    pass

class InvalidResponse(ValueError):
    pass
