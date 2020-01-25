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

static struct node* free_list_head[13]={};
static unsigned long long help=~0xfff;

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

int round_up(int n)
{
	int c = 0;
	if(((n-1)&n)==0)
		c--;
	while ( n )
	{
		c++;
		n>>=1;
	}
	return c;
}

void insert_node(int power,void* ptr)
{
	struct node* my_node = (struct node*)ptr;
	my_node -> next = NULL;
	my_node -> prev = NULL;

	if ( free_list_head[power] != NULL )
	{
		// insert node in free list
		my_node -> next = free_list_head[power];
		free_list_head[power] -> prev = my_node;
	}
	free_list_head[power] = my_node;
}

void delete_node(int power,struct node** temp)
{
	if ( (*temp) -> next == NULL ) 
	{
		if ( (*temp) -> prev == NULL )
		{
			free_list_head[power] = NULL;
			(*temp) = NULL;
		}
		else
		{
			(*temp) -> prev -> next = NULL;
			(*temp) = NULL;
		}
	}
	else if ( (*temp) -> prev == NULL)
	{
		free_list_head[power] = (*temp) -> next;
		(*temp)= (*temp) -> next;
		(*temp) -> prev = NULL;
	}
	else
	{
		(*temp) -> prev -> next = (*temp) -> next;
		(*temp) -> next -> prev = (*temp) -> prev;
		(*temp) = (*temp) -> next;
	}
}

void myfree(void *ptr)
{
	// get page metadata of given ptr 
	struct metadata* metadata_address = (struct metadata*)((unsigned long)(ptr)& help);

	// size of ptr > 4080
	if ( metadata_address -> allocation_size >= PAGE_SIZE)
	{
		free_ram(metadata_address,metadata_address -> allocation_size);
	}
	else
	{
		//updating page metadata
		metadata_address -> free_bytes_available += metadata_address -> allocation_size;
		
		int bucket_size = metadata_address -> allocation_size;
		int power = round_up(bucket_size);

		// whole page is free
		if (metadata_address -> free_bytes_available == 4080 )
		{
			// number of nodes to be deleted from free_list to free the page
			int num_del = (PAGE_SIZE - 16) / bucket_size - 1;

			struct node* temp = free_list_head[power];

			while ( num_del )
			{
				if ( (struct metadata*)((unsigned long long)(temp)& help) == metadata_address )
				{
					num_del--;
					delete_node(power,&temp);
				}
				else
					temp = temp -> next;
			}
			free_ram(metadata_address,PAGE_SIZE);
		}
		else
		{
			insert_node(power,ptr);
		}
	}
}

void *mymalloc(size_t size)
{
	int power = 0, bucket_size, temp_size = size;
	if ( size > 4080 )
		temp_size += 16; // 16 bytes for metadata
	if(temp_size == 8)
		temp_size = 16;

	//nearest power of 2 greater than or equal to size
	power += round_up(temp_size);
	bucket_size = 1 << power;

	if(size > 4080)
	{
		void* page =  alloc_from_ram(bucket_size);
		struct metadata* page_data = (struct metadata*)page;
		page_data -> allocation_size = bucket_size;
		page_data -> free_bytes_available = bucket_size - 16;  // using -1 as a flag to detect asked size was more than  4080 bytes
		return page + 16;
	}
	else
	{
		if( free_list_head[power] == NULL ) //no memory object present of required size in free list
		{
			void* page =  alloc_from_ram(PAGE_SIZE);
			struct metadata* page_data = (struct metadata*) page;
			// details of page metadata
			if( bucket_size == 4096 )
				bucket_size = 4080;
			page_data -> allocation_size = bucket_size;
			page_data -> free_bytes_available = 4080;

			int available_size = 4080;
			// insert node in free list
			for( void* i = page + 16 ;available_size >= bucket_size ; i += bucket_size , available_size -= bucket_size)
			{
				insert_node(power,i);
			}

		}
		// removing a node from free list and returning it
		struct node* ptr = free_list_head[power];
		struct metadata* _metadata = (struct metadata*)((unsigned long long)(ptr)& help);  // page metadata of the first memory object present in the free list
		_metadata -> free_bytes_available -= bucket_size;

		assert(_metadata->free_bytes_available >=0 );

		free_list_head[power] = ptr -> next; // changing free list head
		if ( free_list_head[power] != NULL )
			free_list_head[power] -> prev = NULL;

		ptr -> next = NULL;
	
		return ptr;
	}
}
