/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Zaster Banker.

    Zaster Banker is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zaster Banker is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Zaster Banker. If not, see <http://www.gnu.org/licenses/>.
*/

.pragma library

// The symbol where there is a common one, the ISO code otherwise.
var currencySymbols = { "EUR": "€", "USD": "$", "GBP": "£", "CHF": "CHF" }

function amount(value, currency) {
    var symbol = currencySymbols[currency] !== undefined ? currencySymbols[currency] : (currency ? currency : "")
    return Number(value).toLocaleCurrencyString(Qt.locale(), symbol)
}

// IBANs are read and compared in groups of four.
function iban(text) {
    return text.replace(/\s/g, "").replace(/(.{4})/g, "$1 ").trim()
}

function date(value) {
    return Qt.formatDate(value, Qt.DefaultLocaleShortDate)
}
