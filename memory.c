#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "memory.h"

#define PAGE_SIZE 4096

static void *alloc_from_ram(size_t size)
{
	assert((size % PAGE_SIZE) == 0 && "size must be multiples of 4096");
	void* base = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (base == MAP_FAILED)
	{
		printf("Unable to allocate RAM space\n");
		exit(0);
	}
	return base;
}

static void free_ram(void *addr, size_t size)
{
	munmap(addr, size);
}

struct node* free_list_head[13]={};

struct node
{
	struct node* prev;
	struct node* next;
};
struct metadata
{
	int allocation_size;
	int free_bytes_available;
};
void myfree(void *ptr)
{
	struct metadata* metadata_address = (struct metadata*)((unsigned long)(ptr)& ~0xfff);
	if ( metadata_address -> allocation_size>=PAGE_SIZE && metadata_address -> free_bytes_available == -1)
	{
		free_ram(metadata_address,metadata_address -> allocation_size);
	}
	else
	{
		metadata_address -> free_bytes_available+=metadata_address -> allocation_size;
		
		int power = 0;
		int bucket_size = metadata_address -> allocation_size;
		while ( bucket_size != 1 )
		{
			power++;
			bucket_size >>= 1;
		}
		bucket_size = metadata_address -> allocation_size;

		struct node* my_node = (struct node*)ptr;
		my_node -> next = NULL;
		my_node -> prev = NULL;

		if ( free_list_head[power] == NULL )
		{
			free_list_head[power] = my_node;
		}
		else
		{
			my_node -> next = free_list_head[power];
			free_list_head[power] -> prev = my_node;
			free_list_head[power] = my_node;
		}

		if (metadata_address -> free_bytes_available == PAGE_SIZE)
		{
			int num_del = (PAGE_SIZE - 16) / bucket_size;
			if ( bucket_size == PAGE_SIZE )
				num_del = 1;

			struct node* temp = free_list_head[power];
			while ( num_del )
			{
				if ( (struct metadata*)((unsigned long)(temp)& ~0xfff) == metadata_address)
				{
					num_del--;
					if ( temp -> next == NULL ) 
					{
						if ( temp -> prev == NULL )
						{
							free_list_head[power] = NULL;
							temp = NULL;
						}
						else
						{
							temp -> prev -> next = NULL;
							temp = temp -> next;
						}
					}
					else if (temp -> prev == NULL)
					{
						free_list_head[power] = temp -> next;
						temp = free_list_head[power];
						temp -> prev = NULL;
					}
					else
					{
						temp -> prev -> next = temp -> next;
						temp -> next -> prev = temp -> prev;
						temp = temp -> next;
					}
				}
				else
					temp = temp -> next;
			}
			free_ram(metadata_address,PAGE_SIZE);
		}

	}
}

void *mymalloc(size_t size)
{
	int power = 0, bucket_size, temp_size = size;
	if ( size > 4080 )
		temp_size += 16;
	if( temp_size & (temp_size - 1) == 0 )
		power--;

	//nearest power of 2 greater than or equal to size
	while ( temp_size != 0 )
	{
		temp_size >>= 1;
		power++;
	}
	bucket_size = 1<<power;

	if(size > 4080)
	{
		void* page =  alloc_from_ram(bucket_size);
		struct metadata* page_data = (struct metadata*)page;
		page_data->allocation_size = bucket_size;
		page_data->free_bytes_available = -1;
		return page + 16;
	}

	else
	{
		if( free_list_head[power] == NULL ) //no memory object of required size
		{
			void* page =  alloc_from_ram(PAGE_SIZE);
			struct metadata* page_data = (struct metadata*)page;
			void* upper_limit=page+PAGE_SIZE -bucket_size;
			if(bucket_size == PAGE_SIZE || bucket_size == 16)
				upper_limit += bucket_size;

			for( void* i = page + 16 ; i < upper_limit ; i += bucket_size)
			{
				struct node* my_node = (struct node*)i;
				my_node -> prev = NULL;
				my_node -> next = NULL;

				if (free_list_head[power] == NULL)
					free_list_head[power] = my_node;
				else
				{
					my_node -> next = free_list_head[power];
					free_list_head[power] -> prev = my_node;
					free_list_head[power] = my_node;
				}
			}
			
			page_data -> allocation_size = bucket_size;
			page_data -> free_bytes_available = 4096;
		}

		struct node* ptr = free_list_head[power];
		struct metadata* metadata_address = (struct metadata*)((unsigned long long)(ptr)& ~0xfff);
		metadata_address -> free_bytes_available -= bucket_size;
		
		free_list_head[power] = ptr -> next;
		if ( free_list_head[power] != NULL )
			free_list_head[power] -> prev = NULL;
		ptr -> next = NULL;
		
		return ptr;
	}
}
