#include "ElemHideBase.h"
#include "CSSPropertyFilter.h"
#include "StringScanner.h"

namespace
{
  void NormalizeWhitespace(DependentString& text, String::size_type& domainsEnd,
      String::size_type& selectorStart)
  {
    // For element hiding filters we only want to remove spaces preceding the
    // selector part. The positions we've determined already have to be adjusted
    // accordingly.

    String::size_type delta = 0;
    String::size_type len = text.length();

    // The first character is guaranteed to be a non-space, the string has been
    // trimmed earlier.
    for (String::size_type pos = 1; pos < len; pos++)
    {
      if (pos == domainsEnd)
        domainsEnd -= delta;

      // Only spaces before selectorStart position should be removed.
      if (pos < selectorStart && text[pos] == ' ')
        delta++;
      else
        text[pos - delta] = text[pos];
    }
    selectorStart -= delta;

    text.reset(text, 0, len - delta);
  }
}

ElemHideBase::ElemHideBase(Type type, const String& text, const ElemHideBaseData& data)
    : ActiveFilter(type, text, false), mData(data)
{
  if (mData.HasDomains())
    ParseDomains(mData.GetDomainsSource(mText), u',');
}

Filter::Type ElemHideBase::Parse(DependentString& text, ElemHideData& data)
{
  StringScanner scanner(text);

  // Domains part
  bool seenSpaces = false;
  while (!scanner.done())
  {
    String::value_type next = scanner.next();
    if (next == u'#')
    {
      data.mDomainsEnd = scanner.position();
      break;
    }

    switch (next)
    {
      case u'/':
      case u'*':
      case u'|':
      case u'@':
      case u'"':
      case u'!':
        return Type::UNKNOWN;
      case u' ':
        seenSpaces = true;
        break;
    }
  }

  seenSpaces |= scanner.skip(u' ');
  bool exception = scanner.skipOne(u'@');
  if (exception)
    seenSpaces |= scanner.skip(u' ');

  String::value_type next = scanner.next();
  if (next != u'#')
    return Type::UNKNOWN;

  // Selector part

  // Selector shouldn't be empty
  seenSpaces |= scanner.skip(u' ');
  if (scanner.done())
    return Type::UNKNOWN;

  data.mSelectorStart = scanner.position() + 1;
  while (!scanner.done())
  {
    switch (scanner.next())
    {
      case u'{':
      case u'}':
        return Type::UNKNOWN;
    }
  }

  // We are done validating, now we can normalize whitespace and the domain part
  if (seenSpaces)
    NormalizeWhitespace(text, data.mDomainsEnd, data.mSelectorStart);
  DependentString(text, 0, data.mDomainsEnd).toLower();

  if (exception)
    return Type::ELEMHIDEEXCEPTION;

  do
  {
    // Is this a CSS property rule maybe?
    DependentString searchString(u"[-abp-properties="_str);
    data.mPrefixEnd = text.find(searchString, data.mSelectorStart);
    if (data.mPrefixEnd == text.npos ||
        data.mPrefixEnd + searchString.length() + 1 >= text.length())
    {
      break;
    }

    data.mRegexpStart = data.mPrefixEnd + searchString.length() + 1;
    char16_t quotation = text[data.mRegexpStart - 1];
    if (quotation != u'\'' && quotation != u'"')
      break;

    data.mRegexpEnd = text.find(quotation, data.mRegexpStart);
    if (data.mRegexpEnd == text.npos || data.mRegexpEnd + 1 >= text.length() ||
      text[data.mRegexpEnd + 1] != u']')
    {
      break;
    }

    data.mSuffixStart = data.mRegexpEnd + 2;
    return Type::CSSPROPERTY;
  } while (false);

  return Type::ELEMHIDE;
}

OwnedString ElemHideBase::GetSelectorDomain() const
{
  /* TODO this is inefficient */
  OwnedString result;
  if (mDomains)
  {
    for (auto it = mDomains->begin(); it != mDomains->end(); ++it)
    {
      if (it->second && !it->first.empty())
      {
        if (!result.empty())
          result.append(u',');
        result.append(it->first);
      }
    }
  }
  return result;
}