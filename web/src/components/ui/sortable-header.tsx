import { Button } from "@/components/ui/button";
import { ArrowUpDown, ArrowUp, ArrowDown } from "lucide-react";

interface SortableHeaderProps {
    column: string;
    label: string;
    currentSort: string;
    sortDirection: 'asc' | 'desc';
    onSort: (column: string) => void;
    className?: string; // Allow passing alignment classes
}

export function SortableHeader({ column, label, currentSort, sortDirection, onSort, className }: SortableHeaderProps) {
    return (
        <Button
            variant="ghost"
            onClick={() => onSort(column)}
            className={`-ml-4 h-8 data-[state=open]:bg-accent ${className}`}
        >
            {label}
            {currentSort === column ? (
                sortDirection === 'asc' ? (
                    <ArrowUp className="ml-2 h-4 w-4" />
                ) : (
                    <ArrowDown className="ml-2 h-4 w-4" />
                )
            ) : (
                <ArrowUpDown className="ml-2 h-4 w-4" />
            )}
        </Button>
    );
}
