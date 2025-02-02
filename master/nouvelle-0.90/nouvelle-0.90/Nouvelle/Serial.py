""" Nouvelle.Serial

Core functionality for Nouvelle's object serialization system, including
the tag, place, and Serializer objects.

The lowercase 'place', 'xml', and 'tag' classes break my naming
convention, but since they aren't really used like conventional classes
I think lowercase makes more sense.
"""
#
# Nouvelle web framework
# Copyright (C) 2003-2004 Micah Dowty <micahjd@users.sourceforge.net>
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

__all__ = ['place', 'xml', 'subcontext', 'tag', 'quote', 'Serializer', 'DocumentOwner']

# Use psyco if we can, as this module is the bottleneck in most web apps
# and the code is small enough that the memory penalty is minimal
try:
    from psyco.classes import __metaclass__
except ImportError:
    pass


class place:
    """A placeholder for data that can be rendered by a document's owner.
       For example, place('title') calls render_title() in the object owning
       the current document, context['owner']. If there is not render_title
       function, this will look for a 'title' attribute to return verbatim.
       """
    def __init__(self, name, *args, **kwargs):
        self.name = name
        self.args = args
        self.kwargs = kwargs

    def render(self, context):
        try:
            f = getattr(context['owner'], 'render_' + self.name)
        except AttributeError:
            return getattr(context['owner'], self.name)
        return f(context, *self.args, **self.kwargs)


class xml(str):
    """A marker indicating data that is already represented in raw XML and
       needs no further processing.
       """
    __slots__ = []


class quote:
    """A wrapper for any serializable object that fully serializes it then
       leaves the result as a string rather than an xml() object, so it is quoted.
       This is useful for rendering HTML as quoted text inside of other XML
       documents, for example. If this is used on normal text, note that the
       text will be quoted twice.
       """
    def __init__(self, item):
        self.item = item


class tag:
    """A renderable XHTML tag, containing other renderable objects.
       If an object enclosed doesn't have a 'render' method, it is casted
       to a string and quoted.

       This class was inspired by nevow, but much simpler. In CIA, dynamically
       rebuilding the code to implement changes at runtime is more important
       than speed, and this avoids much of the complexity (and power) of
       nevow's ISerializable.

       Attributes are quoted and converted to tag attributes. Leading underscores
       are removed, so they can be used to define attributes that are reserved
       words in Python.
       """
    def __init__(self, name, **attributes):
        self.name = name
        self.content = []
        self._setAttributes(attributes)

    def __call__(self, name=None, **attributes):
        """A tag instance can be called just like the tag
           class, so any tag instance can be used in place
           of 'tag' to provide default values.
           """
        if name is None:
            name = self.name
        attrs = dict(self.attributes)
        attrs.update(attributes)
        return tag(name, **attrs)[ self.content ]

    def _stringizeAttributes(self, attributes):
        """Return a string representation of the given attribute dictionary,
           with a leading space if there are any attributes. Returns the empty
           string if no attributes were given.
           """
        s = ''
        for key, value in attributes.iteritems():
            if key[0] == '_':
                key = key[1:]
            if value is not None:
                s += ' %s="%s"' % (key, escapeToXml(str(value), True))
        return s

    def _setAttributes(self, attributes):
        """Change this tag's attributes, rerendering the opening and closing text"""
        self.attributes = attributes
        attrString = self._stringizeAttributes(attributes)
        self.renderedOpening = xml('<%s%s>' % (self.name, attrString))
        self.renderedEmpty = xml('<%s%s />' % (self.name, attrString))
        self.renderedClosing = xml('</%s>' % self.name)

    def __getitem__(self, content):
        """Overloads the [] operator used, in Tag, to return a new tag
           with the given content
           """
        newTag = tag(self.name, **self.attributes)
        newTag.content = content
        return newTag

    def render(self, context=None):
        if self.content in ('', [], ()):
            return self.renderedEmpty
        else:
            return [self.renderedOpening, self.content, self.renderedClosing]


class subcontext:
    """A wrapper for any serializable object that makes note of a modified
       context to serialize all content in. This can be used like a tag,
       with content in [] and context changes in ().

       It is not enforced, but normally subcontexts should be immutable.
       """
    def __init__(self, content=None, **modifications):
        self.content = content
        self.modifications = modifications

    def __call__(self, content=None, **modifications):
        """Like tags, instances can be used as templates for creating other
           subcontexts. This returns a new distinct subcontext based on
           the current one.
           """
        if content is None:
            content = self.content
        newMods = dict(self.modifications)
        newMods.update(modifications)
        return subcontext(content, **newMods)

    def __getitem__(self, content):
        """Returns a new subcontext, like this one but with the indicated content"""
        return subcontext(content, **self.modifications)


def escapeToXml(text, isAttrib=0):
    text = text.replace("&", "&amp;")
    text = text.replace("<", "&lt;")
    text = text.replace(">", "&gt;")
    if isAttrib == 1:
        text = text.replace("'", "&apos;")
        text = text.replace("\"", "&quot;")
    return text


class Serializer:
    """Convert arbitrary objects to xml markers recursively. Renderable objects
       are allowed to render themselves, other types must have renderers looked
       up in this class's render_* methods.
       """
    def __init__(self):
        self.cache = {}

    def render(self, obj, context={}):
        # We cache rendering functions based on obj.__class__.
        # This assumes that classes won't spontaneously
        # grow a render() method, which is safe enough for our purposes.
        # If you know otherwise, you can just create a new Serializer or
        # invalidate the cache on this one.
        try:
            f = self.cache[obj.__class__]
        except KeyError:
            f = self.lookupRenderer(obj.__class__)
            self.cache[obj.__class__] = f
        return f(obj, context)

    def lookupRenderer(self, cls):
        """Look up a rendering function for the given class"""
        if hasattr(cls, 'render'):
            return self.render_renderable
        try:
            return getattr(self, 'render_' + cls.__name__)
        except AttributeError:
            return self.render_other

    def render_xml(self, obj, context):
        return obj

    def render_list(self, obj, context):
        return xml(''.join([self.render(o, context) for o in obj]))

    def render_tuple(self, obj, context):
        return self.render_list(obj, context)

    def render_quote(self, obj, context):
        return xml(escapeToXml(str(self.render(obj.item, context))))

    def render_function(self, obj, context):
        return self.render(obj(context), context)

    def render_instancemethod(self, obj, context):
        return self.render(obj(context), context)

    def render_renderable(self, obj, context):
        return self.render(obj.render(context), context)

    def render_subcontext(self, obj, context):
        newContext = dict(context)
        newContext.update(obj.modifications)
        return self.render(obj.content, newContext)

    def render_other(self, obj, context):
        return xml(escapeToXml(str(obj)))


class DocumentOwner(object):
    """A base class defining a 'render' function for objects that own a document"""
    serializerFactory = Serializer

    def render(self, context={}):
        return subcontext(self.document, owner=self)

### The End ###
