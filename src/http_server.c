#include "http_server.h"
#include "http_server_p.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

void
http_server_add_route(http_server_t *server, http_method method, const char *path, http_handler_t handler)
{
  if (server->route_count >= MAX_ROUTES)
  {
    return;
  }

  http_route_t *route = &server->routes[server->route_count++];
  route->method = method;
  route->path = path;
  route->path_len = strlen(path);
  route->handler = handler;
}

// TODO: wildcard path
http_response_t
http_server_handle_request(http_server_t *server, http_request_t *req)
{
  normalize_uri(req);

  for (size_t i = 0; i < server->route_count; i++)
  {
    http_route_t *route = &server->routes[i];

    if (route->method != req->method)
    {
      continue;
    }

    if (route->path_len != req->uri_len)
    {
      continue;
    }

    if (strncmp(route->path, req->uri, route->path_len) == 0)
    {
      return route->handler(req);
    }
  }

  return (http_response_t){
      .status = HTTP_STATUS_NOT_FOUND,
      .body = "Not Found",
      .body_len = strlen("Not Found")};
}

static int
hex_val(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  else if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }
  else if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }
  else
  {
    return -1;
  }
}

ssize_t url_decode(char *str, size_t len)
{
  // for implace ptr
  size_t r = 0, w = 0;

  while (r < len)
  {
    if (str[r] == '%')
    {
      if (r + 2 >= len)
      {
        return -1;
      }

      int h1 = hex_val(str[r+1]);
      int h2 = hex_val(str[r+2]);
      if (h1 < 0 || h2 < 0)
      {
        return -1;
      }

      char decoded = (char)((h1 << 4) | h2);
      if (decoded == '\0')
      {
        return -1;
      }

      str[w++] = decoded;
      r += 3;
    }
    else
    {
      str[w++] = str[r++];
    }
  }

  return (ssize_t)w;
}

ssize_t normalize_path(char *path, size_t len)
{
  if (len == 0 || path[0] != '/')
  {
    return -1;
  }

  size_t r = 0, w = 0;

  while (r < len)
  {
    path[w++] = '/';

    // skip like "////"
    while (r < len && path[r] == '/')
    {
      r++;
    }

    size_t seg_start = r;
    while (r < len && path[r] != '/')
    {
      r++;
    }
    size_t seg_len = r - seg_start;

    if (seg_len == 0)
    {
      // finish path
      break;
    }

    if (seg_len == 1 && path[seg_start] == '.')
    {
      // remove last "/" (because ignore this segment)
      w--;
      continue;
    }

    if (seg_len == 2 && path[seg_start] == '.' && path[seg_start + 1] == '.')
    {
      w--;
      if (w == 0)
      {
        // directory traversal
        return -1;
      }

      while (w > 0 && path[w - 1] != '/')
      {
        w--;
      }
      if (w > 0)
      {
        w--;
      }

      continue;
    }

    for (size_t i = 0; i < seg_len; i++)
    {
      path[w++] = path[seg_start + i];
    }
  }

  if (w == 0)
  {
    path[w++] = '/';
  }

  return (ssize_t)w;
}

int
normalize_uri(http_request_t *request)
{
  const char *path = request->uri;
  size_t path_len = request->uri_len;

  // asterisk form
  if (path_len == 1 && path[0] == '*')
  {
    return 0;
  }

  // absolute form
  if (path_len >= 7 && strncasecmp(path, "http://", 7) == 0)
  {
    path += 7;
    path_len -= 7;

    const char *slash = memchr(path, '/', path_len);
    if (slash)
    {
      path_len = (slash - path);
      path = slash;
    }
    else
    {
      path = "/";
      path_len = 1;
    }
  }
  else if (path_len >= 8 && strncasecmp(path, "https://", 8) == 0)
  {
    path += 8;
    path_len -= 8;

    const char *slash = memchr(path, '/', path_len);
    if (slash)
    {
      path_len = (slash - path);
      path = slash;
    }
    else
    {
      path = "/";
      path_len = 1;
    }
  }

  // TODO: authority form

  request->uri = path;
  request->uri_len = path_len;

  return 0;
}