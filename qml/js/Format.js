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

function currencySymbol(currency) {
    return currencySymbols[currency] !== undefined ? currencySymbols[currency] : (currency ? currency : "")
}

function amount(value, currency) {
    return Number(value).toLocaleCurrencyString(Qt.locale(), currencySymbol(currency))
}

// As many decimals as the number has, at most six: fund units and the prices of
// cheap shares go well beyond cents, and rounding them would misstate them.
function decimals(value) {
    var text = Number(value).toFixed(6).replace(/0+$/, "")
    return text.length - text.indexOf(".") - 1
}

function units(value) {
    return Number(value).toLocaleString(Qt.locale(), "f", decimals(value))
}

function price(value, currency) {
    var places = decimals(value)
    return places <= 2 ? amount(value, currency)
                       : Number(value).toLocaleString(Qt.locale(), "f", places) + " " + currencySymbol(currency)
}

// IBANs are read and compared in groups of four.
function iban(text) {
    return text.replace(/\s/g, "").replace(/(.{4})/g, "$1 ").trim()
}

function date(value) {
    return Qt.formatDate(value, Qt.DefaultLocaleShortDate)
}
